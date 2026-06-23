/**
 * @file    ds18b20.h
 * @brief   DS18B20 1-Wire digital temperature sensor driver (LL GPIO).
 * @author  ltc1342
 * @date    2026-07-01
 *
 * @note    Requires a 4.7 kΩ external pull-up on the data line.
 *          GPIO must be pre-configured as open-drain output (or toggled
 *          between OUTPUT and INPUT modes – see implementation).
 *          Uses timing.h for µs/ms delays.
 *
 *          Typical usage (blocking):
 *          @code
 *            Ds18b20Handle_t   soil_sensor;
 *            Ds18b20Config_t   cfg = { GPIOB, LL_GPIO_PIN_4 };
 *            ds18b20_init(&soil_sensor, &cfg);
 *
 *            float temp_c;
 *            if (ds18b20_read_blocking(&soil_sensor, &temp_c) == SENSOR_OK) {
 *                // use temp_c
 *            }
 *          @endcode
 *
 *          Non-blocking flow:
 *          @code
 *            ds18b20_start_conversion(&soil_sensor);
 *            // ... do other work for at least DS18B20_CONVERSION_MS ...
 *            ds18b20_read(&soil_sensor, &temp_c);
 *          @endcode
 */

#ifndef DS18B20_H_
#define DS18B20_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_ll_gpio.h"
#include "app_types.h"

/* ============================================================================
 *   CONSTANTS
 * ============================================================================ */

/** @brief Bytes in DS18B20 scratchpad (includes CRC byte). */
#define DS18B20_SCRATCHPAD_SIZE    9U

/** @brief Maximum 12-bit temperature conversion time [ms]. */
#define DS18B20_CONVERSION_MS    750U

/** @brief Temperature returned on CRC error or sensor absence. */
#define DS18B20_INVALID_TEMP    -999.0f

/* ============================================================================
 *   TYPES
 * ============================================================================ */

/**
 * @brief  DS18B20 hardware pin configuration.
 */
typedef struct {
    GPIO_TypeDef *port;   /**< GPIO port (e.g. GPIOB)              */
    uint32_t      pin;    /**< LL pin mask (e.g. LL_GPIO_PIN_4)    */
} Ds18b20Config_t;

/**
 * @brief  DS18B20 driver handle.
 * @note   Caller allocates storage (static recommended).
 */
typedef struct {
    Ds18b20Config_t config;         /**< GPIO configuration              */
    bool            initialized;    /**< Guard against unInit use        */
    float           last_temp_c;    /**< Last successful reading [°C]    */
    SensorError_t   last_error;     /**< Last operation result           */
} Ds18b20Handle_t;

/* ============================================================================
 *   API
 * ============================================================================ */

/**
 * @brief  Initialise DS18B20 handle and release the 1-Wire bus.
 * @param  handle  Caller-supplied handle (must not be NULL).
 * @param  config  GPIO pin configuration (must not be NULL).
 * @return ERR_NONE on success, ERR_INVALID_PARAM on NULL arg.
 */
ErrorCode_t ds18b20_init(Ds18b20Handle_t       *handle,
                          const Ds18b20Config_t *config);

/**
 * @brief  Issue a CONVERT_T command (non-blocking – returns immediately).
 * @param  handle  Initialised handle.
 * @return SENSOR_OK        – conversion started successfully.
 *         SENSOR_NOT_FOUND – no device response (check wiring / pull-up).
 * @note   Wait at least DS18B20_CONVERSION_MS before calling ds18b20_read().
 */
SensorError_t ds18b20_start_conversion(Ds18b20Handle_t *handle);

/**
 * @brief  Read scratchpad after conversion has completed.
 * @param  handle    Initialised handle.
 * @param  temp_out  Destination for temperature in °C (must not be NULL).
 * @return SENSOR_OK        – valid temperature written to *temp_out.
 *         SENSOR_CRC_ERROR – scratchpad CRC mismatch; *temp_out unchanged.
 *         SENSOR_NOT_FOUND – bus reset found no device.
 */
SensorError_t ds18b20_read(Ds18b20Handle_t *handle, float *temp_out);

/**
 * @brief  Blocking convenience wrapper: start conversion, wait, then read.
 * @param  handle    Initialised handle.
 * @param  temp_out  Destination for temperature in °C (must not be NULL).
 * @return Same as ds18b20_read().
 * @note   Blocks for DS18B20_CONVERSION_MS (750 ms).
 */
SensorError_t ds18b20_read_blocking(Ds18b20Handle_t *handle, float *temp_out);

/**
 * @brief  Return the error status of the most recent operation.
 * @param  handle  Handle to inspect.
 * @return Cached SensorError_t, or SENSOR_NOT_FOUND if handle is NULL.
 */
SensorError_t ds18b20_get_last_error(const Ds18b20Handle_t *handle);

/**
 * @brief  Return the most recently read temperature (cached).
 * @param  handle  Handle to inspect (must not be NULL).
 * @return Cached temperature [°C], or DS18B20_INVALID_TEMP if uninitialised.
 */
float ds18b20_get_last_temp(const Ds18b20Handle_t *handle);

#ifdef __cplusplus
}
#endif

#endif /* DS18B20_H_ */
