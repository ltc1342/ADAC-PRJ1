/**
 * @file web_server.h
 * @brief Level 3: Local HTTP server — monitoring dashboard and control.
 *
 * Endpoints:
 *
 *  GET  /           → Full HTML dashboard page (auto-refreshes every 2 s)
 *  GET  /api/data   → JSON snapshot: {"temp":x,"hum":x,"light":x,...}
 *  POST /api/cmd    → JSON body {"cmd":"PUMP_ON"} → command to STM32
 *
 * The HTML page is embedded in Flash (.rodata) as a C string — no SD card
 * or SPIFFS needed.  JavaScript fetch() polls /api/data every 2 seconds.
 *
 * Access the dashboard at:  http://<ESP32_IP>/
 * (IP is printed to serial log after Wi-Fi connects)
 */

#pragma once

#include "esp_err.h"

/**
 * @brief Start the HTTP server and register all URI handlers.
 *
 * Call after wifi_manager_init() returns ESP_OK so that the IP
 * address is already assigned.
 *
 * @return ESP_OK on success, ESP_FAIL if httpd_start() fails.
 */
esp_err_t web_server_start(void);

/**
 * @brief Stop the HTTP server and release all resources.
 */
void web_server_stop(void);
