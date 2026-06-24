/**
 * @file main.c
 * @brief Smart Farm ESP32-WROOM-32 Gateway — application entry point.
 *
 * Boot sequence (normal mode, ENABLE_DEEP_SLEEP = 0):
 * ┌─────────────────────────────────────────────────┐
 * │ 1. nvs_flash_init()        — non-volatile store  │
 * │ 2. esp_netif_init()        — TCP/IP stack        │
 * │ 3. esp_event_loop_create() — default event loop  │
 * │ 4. sensor_store_init()     — mutex + zero state  │
 * │ 5. uart_handler_init()     — UART2 ISR + task    │
 * │ 6. wifi_manager_init()     — STA connect (block) │
 * │ 7. mqtt_handler_init()     — broker connect       │
 * │ 8. web_server_start()      — HTTP dashboard       │
 * │ 9. run_normal_loop()       — publish every 1 s   │
 * └─────────────────────────────────────────────────┘
 *
 * Boot sequence (deep-sleep mode, ENABLE_DEEP_SLEEP = 1):
 * ┌─────────────────────────────────────────────────┐
 * │ Steps 1–8 same as above                         │
 * │ 9. run_deep_sleep_cycle()                        │
 * │    └─ wait ≤ ACTIVE_WINDOW_MS for UART frame     │
 * │    └─ mqtt_publish_sensors()                     │
 * │    └─ power_manager_deep_sleep()  ← no return   │
 * └─────────────────────────────────────────────────┘
 *
 * Task map after boot:
 *   Core 0: Wi-Fi/MQTT internal tasks, HTTP server, heartbeat_task
 *   Core 1: uart_rx_task (pinned, high priority)
 *   app_main: runs on core 0, becomes the publish loop (or deep-sleep)
 */

#include "app_config.h"
#include "sensor_data.h"
#include "sensor_store.h"
#include "uart_handler.h"
#include "wifi_manager.h"
#include "mqtt_handler.h"
#include "web_server.h"
#include "power_manager.h"

#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MAIN";

/* ── Helper: NVS initialisation ──────────────────────────────────────────── */

/**
 * @brief Initialise NVS flash, erasing and re-initialising if the
 *        partition is full or was written by a different firmware version.
 */
static void nvs_init(void)
{
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition issue — erasing and re-initialising");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS initialised");
}

/* ── Deep-sleep one-shot cycle (Level 4) ─────────────────────────────────── */

#if ENABLE_DEEP_SLEEP
/**
 * @brief Single publish-then-sleep cycle.
 *
 * Waits up to ACTIVE_WINDOW_MS for a valid UART frame from STM32, publishes
 * it, then enters deep sleep.  If no frame arrives in time, sleep is entered
 * anyway to maintain the power budget.
 *
 * This function does NOT return (power_manager_deep_sleep() reboots the chip).
 */
static void run_deep_sleep_cycle(void)
{
    /* Log wakeup cause on every boot-from-sleep */
    if (power_manager_is_wakeup()) {
        power_manager_log_wakeup();
    }

    /* Poll sensor store until a valid frame arrives or the window expires */
    sensor_data_t data   = {0};
    TickType_t    end    = xTaskGetTickCount() + pdMS_TO_TICKS(ACTIVE_WINDOW_MS);

    while (!data.valid && xTaskGetTickCount() < end) {
        sensor_store_get(&data);
        if (!data.valid) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    if (data.valid) {
        esp_err_t rc = mqtt_publish_sensors(&data);
        if (rc == ESP_OK) {
            ESP_LOGI(TAG, "Published sensor data before sleep");
            /* Allow MQTT outbox to flush before killing Wi-Fi */
            vTaskDelay(pdMS_TO_TICKS(200));
        } else {
            ESP_LOGW(TAG, "Publish failed (rc=%d), sleeping anyway", rc);
        }
    } else {
        ESP_LOGW(TAG, "No sensor data within %d ms active window",
                 ACTIVE_WINDOW_MS);
    }

    /* Power down — does NOT return */
    power_manager_deep_sleep();
}
#endif /* ENABLE_DEEP_SLEEP */

/* ── Normal continuous publish loop ──────────────────────────────────────── */

/**
 * @brief Continuous sensor-publish loop (Level 1 normal mode).
 *
 * Reads sensor_store every 1 second and publishes to MQTT if connected
 * and data is valid.  The MQTT client's internal reconnect logic handles
 * temporary broker outages transparently.
 *
 * This function never returns — it is the "main thread" of the application.
 */
static void run_normal_loop(void)
{
    ESP_LOGI(TAG, "Entering normal publish loop (1 s interval)");

    for (;;) {
        sensor_data_t data = {0};
        sensor_store_get(&data);

        if (data.valid) {
            esp_err_t rc = mqtt_publish_sensors(&data);
            if (rc == ESP_OK) {
                ESP_LOGI(TAG,
                         "Published → T=%.1f°C H=%.1f%% L=%u lux "
                         "S=%u%% Pump=%s Mist=%s",
                         data.temperature, data.humidity,
                         (unsigned)data.light,
                         (unsigned)data.soil_moisture,
                         data.pump_status ? "ON" : "OFF",
                         data.mist_status ? "ON" : "OFF");
            }
        } else {
            ESP_LOGD(TAG, "Waiting for first UART frame from STM32...");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ── app_main ─────────────────────────────────────────────────────────────── */

/**
 * @brief Application entry point — called by ESP-IDF after chip boot.
 *
 * Follows the documented boot sequence in the file header.
 * Uses ESP_ERROR_CHECK() for every critical initialisation step so that
 * a misconfiguration is caught early with a clear error message and
 * reboot rather than silent misbehaviour.
 */
void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, " Smart Farm ESP32 Gateway  v1.0.0");
    ESP_LOGI(TAG, " ESP-IDF %s", esp_get_idf_version());
    ESP_LOGI(TAG, " Deep-sleep mode: %s",
             ENABLE_DEEP_SLEEP ? "ENABLED" : "DISABLED");
    ESP_LOGI(TAG, "========================================");

    /* ── 1. Non-volatile storage ── */
    nvs_init();

    /* ── 2 & 3. TCP/IP + default event loop (required by Wi-Fi & MQTT) ── */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* ── 4. Sensor store (mutex must exist before UART task writes to it) ── */
    ESP_ERROR_CHECK(sensor_store_init());

    /* ── 5. UART2 ↔ STM32 (starts ISR + uart_rx_task on core 1) ── */
    ESP_ERROR_CHECK(uart_handler_init());

    /* ── 6. Wi-Fi STA (blocks until connected or retries exhausted) ── */
    esp_err_t wifi_ret = wifi_manager_init();
    if (wifi_ret != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi failed — restarting in 5 s");
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
    }

    /* ── 7. MQTT client (async — does not block until connected) ── */
    ESP_ERROR_CHECK(mqtt_handler_init());

    /* ── 8. HTTP server (Level 3) ── */
    ESP_ERROR_CHECK(web_server_start());

    /* ── 9. Main task: publish loop or deep-sleep cycle ── */
#if ENABLE_DEEP_SLEEP
    run_deep_sleep_cycle();   /* Does NOT return */
#else
    run_normal_loop();        /* Does NOT return */
#endif

    /* Should never reach here */
    ESP_LOGE(TAG, "Unexpected return from main loop — restarting");
    esp_restart();
}
