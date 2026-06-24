/**
 * @file wifi_manager.c
 * @brief Wi-Fi STA manager using the ESP-IDF event-loop architecture.
 *
 * Design notes:
 *  - All Wi-Fi state changes are handled in wifi_event_handler(), which
 *    runs in the system event task — never block inside it.
 *  - An EventGroup with two bits (CONNECTED / FAIL) allows any task to
 *    synchronise on the connection result via wifi_manager_wait_connected().
 *  - Retry counter is reset on each successful connection so that a
 *    temporary AP outage does not permanently disable reconnect.
 *  - WPA2-PSK is the minimum required auth mode; open networks are
 *    rejected for security.
 */

#include "wifi_manager.h"
#include "app_config.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include <string.h>

static const char *TAG = "WIFI_MANAGER";

/* ── EventGroup bits ─────────────────────────────────────────────────────── */
#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1

/* ── Module-local state ──────────────────────────────────────────────────── */
static EventGroupHandle_t s_wifi_eg      = NULL;
static int                s_retry_count  = 0;
static volatile bool      s_connected    = false;
static esp_timer_handle_t s_reconnect_timer = NULL;

static void on_reconnect_timer(void *arg)
{
    ESP_LOGI(TAG, "Timer expired, calling esp_wifi_connect...");
    esp_wifi_connect();
}

/* ── Event handler (runs in ESP-IDF system event task, not app task) ─────── */

/**
 * @brief Unified Wi-Fi + IP event handler registered on the default loop.
 *
 * @param arg       Unused user argument.
 * @param base      Event base: WIFI_EVENT or IP_EVENT.
 * @param id        Event ID within the base.
 * @param event_data  Event-specific payload (cast per event type).
 */
static void wifi_event_handler(void        *arg,
                                esp_event_base_t base,
                                int32_t      id,
                                void        *event_data)
{
    if (base == WIFI_EVENT) {
        switch (id) {

        /* STA interface started → kick off first connect attempt */
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "STA started, connecting to \"%s\"...", WIFI_SSID);
            esp_wifi_connect();
            break;

        /* Association succeeded (but no IP yet) */
        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "Associated with AP");
            s_retry_count = 0;  /* Reset on each successful association */
            break;

        /* Disconnected — retry or signal failure */
        case WIFI_EVENT_STA_DISCONNECTED: {
            const wifi_event_sta_disconnected_t *disc =
                (wifi_event_sta_disconnected_t *)event_data;

            s_connected = false;
            ESP_LOGW(TAG, "Disconnected (reason=%u)", disc->reason);

            if (s_retry_count < WIFI_RECONNECT_MAX) {
                s_retry_count++;
                ESP_LOGI(TAG, "Reconnecting... attempt %d/%d (delayed by %d ms)",
                         s_retry_count, WIFI_RECONNECT_MAX, WIFI_RECONNECT_DELAY_MS);
                /* Start one-shot timer to connect non-blocking */
                if (s_reconnect_timer != NULL) {
                    esp_timer_stop(s_reconnect_timer);
                    esp_timer_start_once(s_reconnect_timer, WIFI_RECONNECT_DELAY_MS * 1000ULL);
                } else {
                    esp_wifi_connect();
                }
            } else {
                ESP_LOGE(TAG, "Giving up after %d retries", WIFI_RECONNECT_MAX);
                xEventGroupSetBits(s_wifi_eg, WIFI_FAIL_BIT);
            }
            break;
        }

        default:
            break;
        }

    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *evt = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&evt->ip_info.ip));
        s_connected   = true;
        s_retry_count = 0;
        /* Signal all waiting tasks */
        xEventGroupSetBits(s_wifi_eg, WIFI_CONNECTED_BIT);
    }
}

/* ── Public API ──────────────────────────────────────────────────────────── */

esp_err_t wifi_manager_init(void)
{
    /* Create reconnect esp_timer */
    const esp_timer_create_args_t reconnect_timer_args = {
        .callback = on_reconnect_timer,
        .name = "wifi_reconnect"
    };
    esp_err_t timer_err = esp_timer_create(&reconnect_timer_args, &s_reconnect_timer);
    if (timer_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create reconnect timer: %d", timer_err);
    }

    /* Create EventGroup for inter-task synchronisation */
    s_wifi_eg = xEventGroupCreate();
    if (s_wifi_eg == NULL) {
        ESP_LOGE(TAG, "EventGroup alloc failed");
        return ESP_ERR_NO_MEM;
    }

    /* Create default STA netif (must call esp_netif_init() before this) */
    esp_netif_create_default_wifi_sta();

    /* Init Wi-Fi with default config (allocates internal buffers) */
    const wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));

    /* Register event handler for both WIFI_EVENT and IP_EVENT bases */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID,
        wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP,
        wifi_event_handler, NULL, NULL));

    /* Build STA config — zero-init first to clear all fields */
    wifi_config_t wifi_cfg = {0};
    strncpy((char *)wifi_cfg.sta.ssid,
            WIFI_SSID, sizeof(wifi_cfg.sta.ssid) - 1);
    strncpy((char *)wifi_cfg.sta.password,
            WIFI_PASS, sizeof(wifi_cfg.sta.password) - 1);

    /* Require at least WPA2 — reject open/WEP networks */
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_cfg.sta.pmf_cfg.capable    = true;
    wifi_cfg.sta.pmf_cfg.required   = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start()); /* Triggers WIFI_EVENT_STA_START */

    /* Block until connected or all retries exhausted */
    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_eg,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,   /* Do NOT clear bits on exit */
        pdFALSE,   /* Wait for ANY bit (OR logic) */
        portMAX_DELAY
    );

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Wi-Fi connected successfully");
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Wi-Fi connection failed");
    return ESP_FAIL;
}

bool wifi_manager_is_connected(void)
{
    return s_connected;
}

esp_err_t wifi_manager_wait_connected(uint32_t timeout_ms)
{
    TickType_t ticks = (timeout_ms == 0)
                       ? portMAX_DELAY
                       : pdMS_TO_TICKS(timeout_ms);

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_eg, WIFI_CONNECTED_BIT,
        pdFALSE, pdFALSE, ticks);

    return (bits & WIFI_CONNECTED_BIT) ? ESP_OK : ESP_ERR_TIMEOUT;
}
