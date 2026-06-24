/**
 * @file mqtt_handler.c
 * @brief MQTT client implementation (ESP-IDF v5 esp-mqtt component).
 *
 * Architecture:
 *  - esp_mqtt_client_start() is non-blocking; the client runs in its
 *    own internal task managed by the esp-mqtt component.
 *  - All MQTT events are dispatched to mqtt_event_handler() via the
 *    default event loop (same thread-safety model as Wi-Fi events).
 *  - A separate heartbeat_task() wakes every HEARTBEAT_PERIOD_MS and
 *    publishes "online" to farm/status (QoS 0, no retain — only the
 *    LWT "offline" is retained so the broker always shows last state).
 *  - Command parsing uses strcmp on a fixed-size topic copy to avoid
 *    dynamic allocation in the event callback.
 */

#include "mqtt_handler.h"
#include "uart_handler.h"
#include "app_config.h"

#include "mqtt_client.h"       /* ESP-IDF v5 esp-mqtt component */
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>
#include <stdio.h>

static const char *TAG = "MQTT_HANDLER";

static esp_mqtt_client_handle_t s_client    = NULL;
static volatile bool            s_connected = false;

/* ── Internal: command resolver ─────────────────────────────────────────── */

/**
 * @brief Map an MQTT topic + payload pair to a farm_cmd_t.
 *
 * Both topic and data are NOT null-terminated in the event struct, so
 * we copy them into fixed local buffers before comparing.
 * Returns CMD_UNKNOWN for any unrecognised combination.
 *
 * @param topic   Raw topic bytes from MQTT event.
 * @param tlen    Topic byte count.
 * @param payload Raw payload bytes.
 * @param plen    Payload byte count.
 */
static farm_cmd_t resolve_command(const char *topic, int tlen,
                                   const char *payload, int plen)
{
    /* Guard against oversized strings to prevent stack overflow */
    if (tlen <= 0 || tlen >= 64 || plen <= 0 || plen >= 16) {
        return CMD_UNKNOWN;
    }

    char t[64];
    char p[16];
    memcpy(t, topic,   (size_t)tlen); t[tlen] = '\0';
    memcpy(p, payload, (size_t)plen); p[plen] = '\0';

    if (strcmp(t, TOPIC_CMD_PUMP) == 0) {
        if (strcmp(p, "ON")  == 0) return CMD_PUMP_ON;
        if (strcmp(p, "OFF") == 0) return CMD_PUMP_OFF;
    } else if (strcmp(t, TOPIC_CMD_MIST) == 0) {
        if (strcmp(p, "ON")  == 0) return CMD_MIST_ON;
        if (strcmp(p, "OFF") == 0) return CMD_MIST_OFF;
    } else if (strcmp(t, TOPIC_CMD_MODE) == 0) {
        if (strcmp(p, "AUTO")   == 0) return CMD_MODE_AUTO;
        if (strcmp(p, "MANUAL") == 0) return CMD_MODE_MANUAL;
    }

    ESP_LOGW(TAG, "Unrecognised: topic=\"%s\" payload=\"%s\"", t, p);
    return CMD_UNKNOWN;
}

/* ── MQTT event handler ──────────────────────────────────────────────────── */

/**
 * @brief Handles all MQTT client events dispatched by the esp-mqtt task.
 *
 * IMPORTANT: This function runs in the MQTT client's internal task.
 * Keep processing fast — do not call blocking functions here.
 * uart_handler_send_cmd() is safe: uart_write_bytes() is non-blocking.
 *
 * @param arg         User argument (unused).
 * @param base        Event base (always MQTT_EVENTS).
 * @param event_id    MQTT event identifier.
 * @param event_data  esp_mqtt_event_handle_t cast.
 */
static void mqtt_event_handler(void            *arg,
                                esp_event_base_t base,
                                int32_t          event_id,
                                void            *event_data)
{
    const esp_mqtt_event_handle_t ev =
        (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {

    /* ── Connected to broker ── */
    case MQTT_EVENT_CONNECTED:
        s_connected = true;
        ESP_LOGI(TAG, "Connected to broker: %s", MQTT_BROKER_URI);

        /* Subscribe to all command topics (QoS 1 for reliable delivery) */
        esp_mqtt_client_subscribe(s_client, TOPIC_CMD_PUMP, MQTT_QOS_1);
        esp_mqtt_client_subscribe(s_client, TOPIC_CMD_MIST, MQTT_QOS_1);
        esp_mqtt_client_subscribe(s_client, TOPIC_CMD_MODE, MQTT_QOS_1);
        ESP_LOGI(TAG, "Subscribed to command topics");

        /* Announce online (retained=1 so broker serves it to late joiners) */
        esp_mqtt_client_publish(s_client, TOPIC_STATUS, "online",
                                6, MQTT_QOS_1, 1 /* retain */);
        break;

    /* ── Disconnected from broker ── */
    case MQTT_EVENT_DISCONNECTED:
        s_connected = false;
        ESP_LOGW(TAG, "Disconnected from broker (auto-reconnect active)");
        break;

    /* ── Subscribed ACK received ── */
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGD(TAG, "Subscribe ACK msg_id=%d", ev->msg_id);
        break;

    /* ── Message received on a subscribed topic (Level 2) ── */
    case MQTT_EVENT_DATA: {
        farm_cmd_t cmd = resolve_command(ev->topic,  ev->topic_len,
                                         ev->data,   ev->data_len);
        if (cmd != CMD_UNKNOWN) {
            ESP_LOGI(TAG, "Command received → forwarding to STM32 (cmd=%d)", cmd);
            uart_handler_send_cmd(cmd);

            /* Acknowledge back to broker */
            esp_mqtt_client_publish(s_client, TOPIC_CMD_ACK, "OK",
                                    2, MQTT_QOS_0, 0);
        }
        break;
    }

    /* ── Publish ACK (QoS 1) ── */
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGD(TAG, "Publish ACK msg_id=%d", ev->msg_id);
        break;

    /* ── Error ── */
    case MQTT_EVENT_ERROR:
        if (ev->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGE(TAG, "TCP transport error: errno=%d",
                     ev->error_handle->esp_tls_last_esp_err);
        }
        break;

    default:
        break;
    }
}

/* ── Heartbeat task ──────────────────────────────────────────────────────── */

/**
 * @brief Publishes "online" to farm/status every HEARTBEAT_PERIOD_MS.
 *
 * Runs on core 0 at priority 3 (lower than UART RX task).
 * If MQTT is not connected the publish is silently skipped.
 *
 * @param arg Unused.
 */
static void heartbeat_task(void *arg)
{
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_PERIOD_MS));

        if (s_connected) {
            esp_mqtt_client_publish(s_client, TOPIC_STATUS, "online",
                                    6, MQTT_QOS_0, 0);
            ESP_LOGD(TAG, "Heartbeat sent");
        }
    }
    /* unreachable */
    vTaskDelete(NULL);
}

/* ── Public API ──────────────────────────────────────────────────────────── */

esp_err_t mqtt_handler_init(void)
{
    /*
     * esp_mqtt_client_config_t uses nested structs in ESP-IDF v5.
     * The LWT (Last Will and Testament) causes the broker to publish
     * "offline" (retained) automatically if the TCP connection drops.
     */
    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.uri = MQTT_BROKER_URI,
        },
        .credentials = {
            .client_id = MQTT_CLIENT_ID,
        },
        .session = {
            .keepalive = MQTT_KEEPALIVE_SEC,
            .last_will = {
                .topic   = TOPIC_STATUS,
                .msg     = "offline",
                .msg_len = 7,
                .qos     = MQTT_QOS_1,
                .retain  = 1,
            },
        },
        .network = {
            .reconnect_timeout_ms = 5000,
        },
    };

    s_client = esp_mqtt_client_init(&mqtt_cfg);
    if (s_client == NULL) {
        ESP_LOGE(TAG, "esp_mqtt_client_init() failed");
        return ESP_FAIL;
    }

    /* Register event handler on the default event loop */
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(
        s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL));

    /* Start the client (non-blocking — connection happens asynchronously) */
    ESP_ERROR_CHECK(esp_mqtt_client_start(s_client));

    /* Start heartbeat task pinned to core 0 */
    xTaskCreatePinnedToCore(heartbeat_task, "mqtt_hb",
                            TASK_HEARTBEAT_STACK, NULL,
                            TASK_HEARTBEAT_PRIORITY, NULL, 0);

    ESP_LOGI(TAG, "MQTT client started → %s (id=%s)",
             MQTT_BROKER_URI, MQTT_CLIENT_ID);
    return ESP_OK;
}

esp_err_t mqtt_publish_sensors(const sensor_data_t *data)
{
    if (!s_connected) {
        ESP_LOGD(TAG, "mqtt_publish_sensors: not connected, skip");
        return ESP_ERR_INVALID_STATE;
    }
    if (!data || !data->valid) {
        return ESP_ERR_INVALID_ARG;
    }

    char buf[24];
    int  rc = 0;

    /* ── Individual topics ── */
    snprintf(buf, sizeof(buf), "%.2f", data->temperature);
    rc |= esp_mqtt_client_publish(s_client, TOPIC_TEMP, buf, 0, MQTT_QOS_0, 0);

    snprintf(buf, sizeof(buf), "%.2f", data->humidity);
    rc |= esp_mqtt_client_publish(s_client, TOPIC_HUM,  buf, 0, MQTT_QOS_0, 0);

    snprintf(buf, sizeof(buf), "%u", (unsigned)data->light);
    rc |= esp_mqtt_client_publish(s_client, TOPIC_LIGHT, buf, 0, MQTT_QOS_0, 0);

    snprintf(buf, sizeof(buf), "%u", (unsigned)data->soil_moisture);
    rc |= esp_mqtt_client_publish(s_client, TOPIC_SOIL, buf, 0, MQTT_QOS_0, 0);

    snprintf(buf, sizeof(buf), "%u", (unsigned)data->pump_status);
    rc |= esp_mqtt_client_publish(s_client, TOPIC_PUMP_STAT, buf, 0, MQTT_QOS_0, 0);

    snprintf(buf, sizeof(buf), "%u", (unsigned)data->mist_status);
    rc |= esp_mqtt_client_publish(s_client, TOPIC_MIST_STAT, buf, 0, MQTT_QOS_0, 0);

    /* ── Combined JSON payload ── */
    char json[128];
    snprintf(json, sizeof(json),
             "{\"temp\":%.2f,\"hum\":%.2f,\"light\":%u,"
             "\"soil\":%u,\"pump\":%u,\"mist\":%u}",
             data->temperature,   data->humidity,
             (unsigned)data->light, (unsigned)data->soil_moisture,
             (unsigned)data->pump_status, (unsigned)data->mist_status);

    rc |= esp_mqtt_client_publish(s_client, TOPIC_SENSORS_JSON,
                                  json, 0, MQTT_QOS_0, 0);

    /* esp_mqtt_client_publish returns msg_id >= 0 on success, -1 on error */
    if (rc < 0) {
        ESP_LOGW(TAG, "One or more publishes failed (rc=%d)", rc);
        return ESP_FAIL;
    }

    return ESP_OK;
}

bool mqtt_handler_is_connected(void)
{
    return s_connected;
}
