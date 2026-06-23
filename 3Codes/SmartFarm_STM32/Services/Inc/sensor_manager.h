/**
 * @file    sensor_manager.h
 * @brief   Sensor collection service – reads all peripherals and
 *          aggregates results into a single SensorData_t snapshot.
 * @author  ltc1342
 * @date    2026-07-01
 *
 * @note    This service is device-agnostic: callers register function
 *          pointers for each sensor instead of passing device handles
 *          directly.  This decouples the service layer from the device
 *          layer and simplifies unit-testing.
 *
 *          Usage example:
 *          @code
 *            static SensorManager_t  sensor_mgr;
 *
 *            // Wrap device APIs into matching signatures (see typedefs)
 *            SensorManagerConfig_t cfg = {
 *                .dht11_handle  = &g_dht11,
 *                .dht11_read    = dht11_adapter_read,   // user-written adapter
 *                .bh1750_handle = &g_bh1750,
 *                .bh1750_read   = bh1750_adapter_read,
 *                .ds18b20_handle = &g_ds18b20,          // NULL to skip
 *                .ds18b20_read   = ds18b20_adapter_read,
 *                .soil_handle   = NULL,                 // ADC via callback
 *                .soil_read     = soil_adc_adapter_read,
 *            };
 *
 *            sensor_manager_init(&sensor_mgr, &cfg);
 *
 *            // In main loop:
 *            sensor_manager_read_all(&sensor_mgr);
 *            const SensorData_t *data;
 *            sensor_manager_get_data(&sensor_mgr, &data);
 *          @endcode
 */

#ifndef SENSOR_MANAGER_H_
#define SENSOR_MANAGER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "app_types.h"

/* ============================================================================
 *   FUNCTION-POINTER TYPEDEFS  (device-agnostic interface)
 * ============================================================================ */

/**
 * @brief  Adapter signature for DHT11: reads temperature AND humidity in one call.
 * @param  handle    Opaque pointer to the device handle (e.g. Dht11Handle_t*).
 * @param  temp_out  Destination for temperature [°C].
 * @param  hum_out   Destination for relative humidity [%RH].
 * @return SENSOR_OK, SENSOR_TIMEOUT, or SENSOR_CRC_ERROR.
 */
typedef SensorError_t (*dht11_read_fn_t)(void  *handle,
                                          float *temp_out,
                                          float *hum_out);

/**
 * @brief  Adapter signature for BH1750: reads illuminance.
 * @param  handle   Opaque pointer to the device handle (e.g. Bh1750Handle_t*).
 * @param  lux_out  Destination for illuminance [lux].
 * @return SENSOR_OK or SENSOR_TIMEOUT.
 */
typedef SensorError_t (*lux_read_fn_t)(void  *handle,
                                        float *lux_out);

/**
 * @brief  Adapter signature for a single-float temperature source (DS18B20).
 * @param  handle    Opaque pointer to device handle (e.g. Ds18b20Handle_t*).
 * @param  temp_out  Destination for temperature [°C].
 * @return SENSOR_OK, SENSOR_NOT_FOUND, or SENSOR_CRC_ERROR.
 */
typedef SensorError_t (*temp_read_fn_t)(void  *handle,
                                         float *temp_out);

/**
 * @brief  Adapter signature for soil moisture ADC reader.
 * @param  handle        Opaque handle (e.g. ADC handle or NULL for HAL global).
 * @param  moisture_out  Destination for soil moisture [0–100 %].
 * @return SENSOR_OK or SENSOR_BUSY.
 */
typedef SensorError_t (*soil_read_fn_t)(void  *handle,
                                         float *moisture_out);

/* ============================================================================
 *   CONFIGURATION  (injected at init)
 * ============================================================================ */

/**
 * @brief  Hardware interface descriptor – one entry per sensor.
 * @note   Set ds18b20_handle / ds18b20_read to NULL to skip soil temperature.
 */
typedef struct {
    void             *dht11_handle;    /**< Opaque Dht11Handle_t*          */
    dht11_read_fn_t   dht11_read;      /**< Adapter function               */

    void             *bh1750_handle;   /**< Opaque Bh1750Handle_t*         */
    lux_read_fn_t     bh1750_read;     /**< Adapter function               */

    void             *ds18b20_handle;  /**< Opaque Ds18b20Handle_t* / NULL */
    temp_read_fn_t    ds18b20_read;    /**< Adapter function   / NULL      */

    void             *soil_handle;     /**< Opaque ADC handle  / NULL      */
    soil_read_fn_t    soil_read;       /**< Adapter function               */
} SensorManagerConfig_t;

/* ============================================================================
 *   MANAGER INSTANCE
 * ============================================================================ */

/**
 * @brief  Sensor manager internal state.
 * @note   Caller allocates this (typically static).
 */
typedef struct {
    SensorManagerConfig_t config;           /**< Registered adapters        */
    SensorData_t          latest;           /**< Most recent aggregated data */
    uint32_t              last_read_ms;     /**< timing_get_ms() at last read */
    bool                  initialized;
} SensorManager_t;

/* ============================================================================
 *   API
 * ============================================================================ */

/**
 * @brief  Initialise sensor manager with hardware adapters.
 * @param  mgr     Manager instance (must not be NULL).
 * @param  config  Adapter configuration (must not be NULL).
 * @return ERR_NONE on success, ERR_INVALID_PARAM on NULL arg or missing
 *         mandatory adapters (dht11, bh1750, soil).
 */
ErrorCode_t sensor_manager_init(SensorManager_t              *mgr,
                                 const SensorManagerConfig_t  *config);

/**
 * @brief  Read all registered sensors and update the internal snapshot.
 * @param  mgr  Initialised manager instance.
 * @return ERR_NONE if at least the mandatory sensors (DHT11, BH1750, soil)
 *         responded without fault.
 *         ERR_HW_INIT_FAIL if all mandatory sensors failed.
 * @note   Individual sensor errors are stored inside SensorData_t.
 */
ErrorCode_t sensor_manager_read_all(SensorManager_t *mgr);

/**
 * @brief  Obtain a read-only pointer to the latest sensor snapshot.
 * @param  mgr      Initialised manager instance.
 * @param  data_out Set to address of the internal SensorData_t (never NULL
 *                  after a successful init).
 * @return ERR_NONE on success, ERR_INVALID_PARAM on NULL arg.
 */
ErrorCode_t sensor_manager_get_data(const SensorManager_t  *mgr,
                                     const SensorData_t    **data_out);

/**
 * @brief  Check whether the latest snapshot is within the freshness window.
 * @param  mgr          Initialised manager instance.
 * @param  interval_ms  Maximum allowed age of the data [ms].
 * @return true  – data age ≤ interval_ms.
 *         false – data is stale or manager not initialised.
 */
bool sensor_manager_is_fresh(const SensorManager_t *mgr,
                              uint32_t               interval_ms);

/**
 * @brief  Force all sensor error fields to SENSOR_OK (for testing only).
 * @param  mgr  Initialised manager instance.
 */
void sensor_manager_reset_errors(SensorManager_t *mgr);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_MANAGER_H_ */
