/**
 * @file    sensor_manager.c
 * @brief   Implementation of sensor aggregation service.
 * @author  Group SmartFarm
 * @date    2026-07-01
 */

#include "sensor_manager.h"
#include "timing.h"
#include "debug_log.h"
#include "ds18b20.h"
#include <string.h>

/* ============================================================================
 *   PRIVATE HELPERS
 * ============================================================================ */

/** Mark mandatory adapters as missing if function pointer is NULL. */
static bool is_mandatory_ready(const SensorManager_t *mgr)
{
    return (mgr->config.dht11_read != NULL) &&
           (mgr->config.bh1750_read != NULL) &&
           (mgr->config.soil_read   != NULL);
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

    /* Copy config */
    memcpy(&mgr->config, config, sizeof(SensorManagerConfig_t));

    /* Initialise latest data to zeros */
    memset(&mgr->latest, 0, sizeof(SensorData_t));
    mgr->latest.is_valid = false;
    mgr->last_read_ms    = 0U;
    mgr->initialized     = true;

    debug_log("sensor_manager: init OK\r\n");
    return ERR_NONE;
}

ErrorCode_t sensor_manager_read_all(SensorManager_t *mgr)
{
    if ((mgr == NULL) || (!mgr->initialized))
    {
        return ERR_INVALID_PARAM;
    }

    if (!is_mandatory_ready(mgr))
    {
        debug_log("sensor_manager: mandatory adapters missing\r\n");
        return ERR_HW_INIT_FAIL;
    }

    SensorData_t *d = &mgr->latest;
    SensorError_t err;

    /* ---- DHT11 ---- */
    if (mgr->config.dht11_read != NULL)
    {
        err = mgr->config.dht11_read(mgr->config.dht11_handle,
                                     &d->temperature,
                                     &d->humidity);
        d->dht11_error = err;
    }
    else
    {
        d->dht11_error = SENSOR_NOT_FOUND;
    }

    /* ---- BH1750 ---- */
    if (mgr->config.bh1750_read != NULL)
    {
        err = mgr->config.bh1750_read(mgr->config.bh1750_handle,
                                      &d->light_lux);
        d->bh1750_error = err;
    }
    else
    {
        d->bh1750_error = SENSOR_NOT_FOUND;
    }

    /* ---- DS18B20 (optional) ---- */
    if (mgr->config.ds18b20_read != NULL)
    {
        err = mgr->config.ds18b20_read(mgr->config.ds18b20_handle,
                                       &d->soil_temperature);
        d->ds18b20_error = err;
    }
    else
    {
        d->soil_temperature = DS18B20_INVALID_TEMP; /* maybe define */
        d->ds18b20_error = SENSOR_OK;
    }

    /* ---- Soil moisture (ADC) ---- */
    if (mgr->config.soil_read != NULL)
    {
        err = mgr->config.soil_read(mgr->config.soil_handle,
                                    &d->soil_moisture);
        d->adc_error = err;
    }
    else
    {
        d->adc_error = SENSOR_NOT_FOUND;
    }

    d->timestamp_ms = timing_get_ms();
    d->is_valid = (d->dht11_error    == SENSOR_OK) &&
                  (d->bh1750_error   == SENSOR_OK) &&
                  (d->adc_error      == SENSOR_OK);

    mgr->last_read_ms = d->timestamp_ms;

    return (d->is_valid) ? ERR_NONE : ERR_HW_INIT_FAIL;
}

ErrorCode_t sensor_manager_get_data(const SensorManager_t  *mgr,
                                     const SensorData_t    **data_out)
{
    if ((mgr == NULL) || (data_out == NULL) || (!mgr->initialized))
    {
        return ERR_INVALID_PARAM;
    }

    *data_out = &mgr->latest;
    return ERR_NONE;
}

bool sensor_manager_is_fresh(const SensorManager_t *mgr, uint32_t interval_ms)
{
    if ((mgr == NULL) || (!mgr->initialized))
    {
        return false;
    }

    uint32_t now = timing_get_ms();
    uint32_t age = now - mgr->last_read_ms;
    return (age <= interval_ms);
}

void sensor_manager_reset_errors(SensorManager_t *mgr)
{
    if ((mgr == NULL) || (!mgr->initialized))
    {
        return;
    }

    mgr->latest.dht11_error    = SENSOR_OK;
    mgr->latest.bh1750_error   = SENSOR_OK;
    mgr->latest.ds18b20_error  = SENSOR_OK;
    mgr->latest.adc_error      = SENSOR_OK;
    mgr->latest.is_valid       = true;
}
