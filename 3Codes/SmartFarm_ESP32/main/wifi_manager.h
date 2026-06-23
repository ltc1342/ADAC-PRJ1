/**
 * @file wifi_manager.h
 * @brief Wi-Fi Station (STA) connection manager.
 *
 * Connection flow driven entirely by the ESP-IDF default event loop:
 *
 *   wifi_manager_init()
 *     └─► esp_wifi_start()
 *           └─► WIFI_EVENT_STA_START     → esp_wifi_connect()
 *                 └─► WIFI_EVENT_STA_CONNECTED
 *                       └─► IP_EVENT_STA_GOT_IP → WIFI_CONNECTED_BIT ✓
 *
 * On disconnect:
 *   WIFI_EVENT_STA_DISCONNECTED → retry (up to WIFI_RECONNECT_MAX)
 *   → all retries exhausted → WIFI_FAIL_BIT ✗
 *
 * Other modules call wifi_manager_wait_connected() to block until the
 * link is up before starting MQTT / HTTP.
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initialise TCP/IP stack, configure STA, and start Wi-Fi.
 *
 * Blocks until an IP address is obtained or all retries are exhausted.
 * Call once from app_main after nvs_flash_init() and esp_netif_init().
 *
 * @return ESP_OK on successful IP assignment, ESP_FAIL otherwise.
 */
esp_err_t wifi_manager_init(void);

/**
 * @brief Query current connection status (non-blocking).
 * @return true if the station has a valid IP address.
 */
bool wifi_manager_is_connected(void);

/**
 * @brief Block the calling task until Wi-Fi is connected.
 *
 * @param timeout_ms  Maximum wait in milliseconds; 0 = wait forever.
 * @return ESP_OK on connect, ESP_ERR_TIMEOUT if timed out.
 */
esp_err_t wifi_manager_wait_connected(uint32_t timeout_ms);
