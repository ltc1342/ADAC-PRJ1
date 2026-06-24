/**
 * @file sensor_data.h
 * @brief Shared sensor data structures and command enumerations.
 *
 * This header is included by ALL modules (uart_handler, mqtt_handler,
 * web_server, sensor_store) to ensure a single definition of the
 * data types that flow through the system.
 *
 * Data pipeline:
 *   STM32 → UART → uart_handler (parse CSV) → sensor_store (write)
 *   sensor_store (read) → mqtt_handler (publish)
 *   sensor_store (read) → web_server  (serve JSON)
 *   mqtt_handler (cmd received) → uart_handler (send string to STM32)
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/* ── Sensor reading (FROM STM32) ──────────────────────────────────────── */

/**
 * @brief Parsed sensor snapshot received from STM32 over UART2.
 *
 * Wire format (CSV, newline-terminated):
 *   "25.3,58,320,45,1,0\n"
 *    temp  hum light soil pump mist
 */
typedef struct {
    float    temperature;   /**< Ambient temperature  (°C)        */
    float    humidity;      /**< Relative humidity    (%RH)       */
    uint16_t light;         /**< Light intensity      (lux)       */
    uint8_t  soil_moisture; /**< Soil moisture        (0–100 %)   */
    uint8_t  pump_status;   /**< Water pump           (1=ON)      */
    uint8_t  mist_status;   /**< Mist/fan             (1=ON)      */
    bool     valid;         /**< Set true after first valid parse  */
} sensor_data_t;

/* ── Commands (FROM MQTT broker → ESP32 → STM32) ─────────────────────── */

/**
 * @brief Farm actuator commands decoded from MQTT payloads.
 */
typedef enum {
    CMD_PUMP_ON     = 0,
    CMD_PUMP_OFF,
    CMD_MIST_ON,
    CMD_MIST_OFF,
    CMD_MODE_AUTO,
    CMD_MODE_MANUAL,
    CMD_UNKNOWN         /**< Sentinel – always last */
} farm_cmd_t;
