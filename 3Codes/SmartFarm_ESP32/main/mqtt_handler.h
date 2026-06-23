/**
 * @file mqtt_handler.h
 * @brief MQTT publish/subscribe handler — Levels 1 & 2.
 *
 * Level 1 — Publish:
 *   Individual topics  : farm/temperature, farm/humidity, …
 *   Combined JSON      : farm/sensors  {"temp":x,"hum":x,…}
 *   Heartbeat          : farm/status   "online" every 30 s
 *
 * Level 2 — Subscribe & forward commands:
 *   farm/commands/pump  "ON"/"OFF"   → CMD_PUMP_ON / CMD_PUMP_OFF
 *   farm/commands/mist  "ON"/"OFF"   → CMD_MIST_ON / CMD_MIST_OFF
 *   farm/commands/mode  "AUTO"/"MANUAL"
 *   farm/commands/ack   ← "OK" acknowledgment sent back to broker
 *
 * LWT (Last Will and Testament):
 *   If ESP32 drops connection unexpectedly, broker publishes
 *   farm/status "offline" (retained, QoS 1) on behalf of the client.
 */

#pragma once

#include "sensor_data.h"
#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief Start the MQTT client and connect to the configured broker.
 *
 * Call after wifi_manager_init() returns ESP_OK.
 * Internally registers event callbacks and starts the heartbeat task.
 *
 * @return ESP_OK on successful client start.
 */
esp_err_t mqtt_handler_init(void);

/**
 * @brief Publish all sensor values to their individual MQTT topics
 *        AND to the combined JSON topic farm/sensors.
 *
 * No-op (returns ESP_ERR_INVALID_STATE) if MQTT is not yet connected
 * or if data->valid is false.
 *
 * @param data  Pointer to a valid sensor snapshot.
 * @return ESP_OK if all publishes were queued successfully.
 */
esp_err_t mqtt_publish_sensors(const sensor_data_t *data);

/**
 * @brief Check MQTT broker connection status.
 * @return true if the client is currently connected.
 */
bool mqtt_handler_is_connected(void);
