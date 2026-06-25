/**
 * @file    sensor_manager.c
 * @brief   Sensor collection service implementation.
 *
 * @details Calls each registered device adapter in sequence, aggregates
 *          results into a single SensorData_t snapshot, and records a
 *          freshness timestamp.  The design is device-agnostic: concrete
 *          sensor types are hidden behind function pointers injected at
 *          sensor_manager_init().
 *
 * @author  ltc1342
 * @date    2026-07-01
 *
 * @note    Timing uses HAL_GetTick() (32-bit ms, wraps every ~49.7 days)
 *          rather than timing_get_ms() (DWT-based, wraps every ~44.7 s at
 *          96 MHz) so that sensor_manager_is_fresh() is reliable over the
 *          full SENSOR_READ_INTERVAL_MS window.
 */

#include "sensor_manager.h"
#include "debug_log.h"
#include "stm32f4xx_hal.h"   /* HAL_GetTick() */

/* ============================================================================
 *   PRIVATE HELPERS
 * ============================================================================ */

/**
 * @brief  Call the DHT11 adapter and update the snapshot.
 * @param  mgr  Initialised manager (caller guarantees non-NULL).
 */
static void read_dht11(SensorManager_t *mgr)
{
    float temp_c = 0.0f;
    float hum_pct = 0.0f;

    if ((mgr->config.dht11_read    == NULL) ||
        (mgr->config.dht11_handle  == NULL))
    {
        mgr->latest.dht11_error = SENSOR_NOT_FOUND;
        return;
    }

    mgr->latest.dht11_error = mgr->config.dht11_read(mgr->config.dht11_handle,
                                                       &temp_c,
                                                       &hum_pct);
    if (mgr->latest.dht11_error == SENSOR_OK)
    {
        mgr->latest.temperature = temp_c;
        mgr->latest.humidity    = hum_pct;
    }
    else
    {
        debug_log("[SensorMgr] DHT11 error: %u\r\n",
                  (unsigned)mgr->latest.dht11_error);
    }
}

/**
 * @brief  Call the BH1750 adapter and update the snapshot.
 * @param  mgr  Initialised manager (caller guarantees non-NULL).
 */
static void read_bh1750(SensorManager_t *mgr)
{
    float lux = 0.0f;

    if ((mgr->config.bh1750_read   == NULL) ||
        (mgr->config.bh1750_handle == NULL))
    {
        mgr->latest.bh1750_error = SENSOR_NOT_FOUND;
        return;
    }

    mgr->latest.bh1750_error = mgr->config.bh1750_read(mgr->config.bh1750_handle,
                                                         &lux);
    if (mgr->latest.bh1750_error == SENSOR_OK)
    {
        mgr->latest.light_lux = lux;
    }
    else
    {
        debug_log("[SensorMgr] BH1750 error: %u\r\n",
                  (unsigned)mgr->latest.bh1750_error);
    }
}

/**
 * @brief  Call the DS18B20 adapter and update the snapshot (optional sensor).
 * @param  mgr  Initialised manager (caller guarantees non-NULL).
 */
static void read_ds18b20(SensorManager_t *mgr)
{
    float temp_c = 0.0f;

    /* DS18B20 is optional – silently skip if not registered */
    if ((mgr->config.ds18b20_read   == NULL) ||
        (mgr->config.ds18b20_handle == NULL))
    {
        mgr->latest.ds18b20_error    = SENSOR_NOT_FOUND;
        mgr->latest.soil_temperature = 0.0f;
        return;
    }

    mgr->latest.ds18b20_error = mgr->config.ds18b20_read(mgr->config.ds18b20_handle,
                                                           &temp_c);
    if (mgr->latest.ds18b20_error == SENSOR_OK)
    {
        mgr->latest.soil_temperature = temp_c;
    }
    else
    {
        debug_log("[SensorMgr] DS18B20 error: %u\r\n",
                  (unsigned)mgr->latest.ds18b20_error);
    }
}

/**
 * @brief  Call the soil moisture adapter and update the snapshot.
 * @param  mgr  Initialised manager (caller guarantees non-NULL).
 */
static void read_soil(SensorManager_t *mgr)
{
    float moisture_pct = 0.0f;

    if (mgr->config.soil_read == NULL)
    {
        mgr->latest.adc_error = SENSOR_NOT_FOUND;
        return;
    }

    /* soil_handle is allowed to be NULL (adapter may use a global ADC handle) */
    mgr->latest.adc_error = mgr->config.soil_read(mgr->config.soil_handle,
                                                    &moisture_pct);
    if (mgr->latest.adc_error == SENSOR_OK)
    {
        mgr->latest.soil_moisture = moisture_pct;
    }
    else
    {
        debug_log("[SensorMgr] Soil ADC error: %u\r\n",
                  (unsigned)mgr->latest.adc_error);
    }
}

/* ============================================================================
 *   PUBLIC API
 * ============================================================================ */

ErrorCode_t sensor_manager_init(SensorManager_t              *mgr,
                                 const SensorManagerConfig_t  *config)
{
    if ((mgr == NULL) || (config == NULL))
    {
        return ERR_INVALID_PARAM;
    }

    /* Mandatory adapters must be registered */
    if ((config->dht11_read  == NULL) ||
        (config->bh1750_read == NULL) ||
        (config->soil_read   == NULL))
    {
        debug_log("[SensorMgr] init failed: mandatory adapter(s) missing\r\n");
        return ERR_INVALID_PARAM;
    }

    mgr->config       = *config;
    mgr->last_read_ms = 0U;
    mgr->initialized  = true;

    /* Zero-initialise snapshot; is_valid starts false until first read */
    mgr->latest          = (SensorData_t){ 0 };
    mgr->latest.is_valid = false;

    debug_log("[SensorMgr] initialised\r\n");
    return ERR_NONE;
}

ErrorCode_t sensor_manager_read_all(SensorManager_t *mgr)
{
    if ((mgr == NULL) || (!mgr->initialized))
    {
        return ERR_INVALID_PARAM;
    }

    /* Sequentially read all registered sensors.
     * Each helper updates the relevant SensorData_t fields and error code.
     * DS18B20 is last because it blocks for 750 ms in blocking mode. */
    read_dht11(mgr);
    read_bh1750(mgr);
    read_soil(mgr);
    read_ds18b20(mgr);   /* 750 ms blocking – always last */

    /* Stamp the snapshot with the wall-clock time of collection */
    mgr->last_read_ms         = HAL_GetTick();
    mgr->latest.timestamp_ms  = mgr->last_read_ms;

    /* Mark valid if at least one mandatory sensor delivered data.
     * Callers can inspect individual error fields for per-sensor status. */
    bool mandatory_ok = (mgr->latest.dht11_error  == SENSOR_OK) ||
                        (mgr->latest.bh1750_error  == SENSOR_OK) ||
                        (mgr->latest.adc_error     == SENSOR_OK);

    mgr->latest.is_valid = mandatory_ok;

    if (!mandatory_ok)
    {
        debug_log("[SensorMgr] all mandatory sensors failed\r\n");
        return ERR_HW_INIT_FAIL;
    }

    return ERR_NONE;
}

ErrorCode_t sensor_manager_get_data(const SensorManager_t  *mgr,
                                     const SensorData_t    **data_out)
{
    if ((mgr == NULL) || (data_out == NULL) || (!mgr->initialized))
    {
        return ERR_INVALID_PARAM;
    }

    /* Return a read-only pointer to the internal snapshot.
     * The caller must not cache this pointer across reads. */
    *data_out = &mgr->latest;
    return ERR_NONE;
}

bool sensor_manager_is_fresh(const SensorManager_t *mgr, uint32_t interval_ms)
{
    if ((mgr == NULL) || (!mgr->initialized))
    {
        return false;
    }

    /* Unsigned subtraction handles 32-bit HAL_GetTick() wrap correctly
     * (valid for intervals up to ~49.7 days). */
    uint32_t age_ms = HAL_GetTick() - mgr->last_read_ms;
    return (age_ms <= interval_ms);
}

void sensor_manager_reset_errors(SensorManager_t *mgr)
{
    if ((mgr == NULL) || (!mgr->initialized))
    {
        return;
    }

    mgr->latest.dht11_error   = SENSOR_OK;
    mgr->latest.bh1750_error  = SENSOR_OK;
    mgr->latest.ds18b20_error = SENSOR_OK;
    mgr->latest.adc_error     = SENSOR_OK;
}
