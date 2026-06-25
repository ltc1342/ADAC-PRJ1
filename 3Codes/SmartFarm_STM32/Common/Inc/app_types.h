/**
 * @file    app_types.h
 * @brief   Global type definitions for the entire project.
 * @author  Group SmartFarm
 * @date    2026-07-01
 *
 * @note    All custom types use PascalCase_t suffix.
 */

#ifndef APP_TYPES_H
#define APP_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 *   ENUMERATIONS (UPPER_SNAKE_CASE)
 * ============================================================================ */

/** @brief Relay control states (Active LOW handled in BSP). */
typedef enum {
    RELAY_OFF = 0U,
    RELAY_ON  = 1U
} RelayState_t;

/** @brief System operating mode. */
typedef enum {
    MODE_AUTO     = 0U,
    MODE_MANUAL   = 1U,
    MODE_SCHEDULE = 2U
} ControlMode_t;

/** @brief Sensor error codes. */
typedef enum {
    SENSOR_OK          = 0U,
    SENSOR_TIMEOUT     = 1U,
    SENSOR_CRC_ERROR   = 2U,
    SENSOR_NOT_FOUND   = 3U,
    SENSOR_BUSY        = 4U
} SensorError_t;

/** @brief System‑wide error codes. */
typedef enum {
    ERR_NONE           = 0U,
    ERR_INVALID_PARAM  = 1U,
    ERR_HW_INIT_FAIL   = 2U,
    ERR_I2C_TX_FAIL    = 3U,
    ERR_I2C_RX_FAIL    = 4U,
    ERR_UART_TX_FAIL   = 5U,
    ERR_UART_RX_FAIL   = 6U,
    ERR_ADC_CONV_FAIL  = 7U
} ErrorCode_t;

/* ============================================================================
 *   STRUCTURES (PascalCase_t)
 * ============================================================================ */

/** @brief All sensor readings in one place. */
typedef struct {
    float temperature;
    float humidity;
    float soil_moisture;
    float light_lux;
    float soil_temperature;     /* optional */
    SensorError_t dht11_error;
    SensorError_t bh1750_error;
    SensorError_t ds18b20_error;
    SensorError_t adc_error;
    uint32_t timestamp_ms;
    bool is_valid;
} SensorData_t;

/** @brief Relay status structure. */
typedef struct {
    RelayState_t pump;
    RelayState_t mist;
} RelayStatus_t;

/** @brief Time structure for RTC / schedule. */
typedef struct {
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} TimeOfDay_t;

typedef struct {
    uint8_t day;
    uint8_t month;
    uint16_t year;
    uint8_t weekday;   // 1=Mon ... 7=Sun
} Date_t;
#ifdef __cplusplus
}
#endif

#endif /* APP_TYPES_H */
