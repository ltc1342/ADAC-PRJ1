/**
 * @file sensor_store.h
 * @brief Thread-safe global sensor data store.
 *
 * Wraps a single sensor_data_t with a FreeRTOS mutex so that the
 * UART RX task (writer) and MQTT / HTTP tasks (readers) can access
 * the latest sensor snapshot without data races.
 *
 * Usage:
 *   sensor_store_init();          // call once in app_main
 *   sensor_store_set(&data);      // called by uart_handler
 *   sensor_store_get(&snapshot);  // called by mqtt_handler, web_server
 */

#pragma once

#include "sensor_data.h"
#include "esp_err.h"

/**
 * @brief Initialise the sensor store (allocates internal mutex).
 *
 * Must be called before any other sensor_store_* function.
 * @return ESP_OK on success, ESP_ERR_NO_MEM if mutex creation fails.
 */
esp_err_t sensor_store_init(void);

/**
 * @brief Overwrite the stored sensor snapshot (ISR-safe caller only
 *        needs to ensure it is NOT called from an ISR directly).
 *
 * Blocks for up to 100 ms waiting for the mutex; silently returns on
 * timeout (UART RX task will retry on the next frame).
 *
 * @param data  Pointer to new sensor_data_t (copied by value).
 */
void sensor_store_set(const sensor_data_t *data);

/**
 * @brief Read the latest sensor snapshot.
 *
 * Blocks for up to 100 ms. If the mutex cannot be acquired, @p out is
 * left unchanged — callers should check out->valid before using data.
 *
 * @param out  Destination sensor_data_t (written by value).
 */
void sensor_store_get(sensor_data_t *out);
