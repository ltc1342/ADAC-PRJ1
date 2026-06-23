/**
 * @file    control_manager.c
 * @brief   Automatic control engine implementation.
 * @author  Group SmartFarm
 * @date    2026-07-01
 */

#include "control_manager.h"
#include "debug_log.h"
#include "timing.h"
#include <string.h>

/* ============================================================================
 *   PRIVATE HELPERS
 * ============================================================================ */

static bool is_auto_mode(ControlMode_t mode)
{
    return (mode == MODE_AUTO) || (mode == MODE_SCHEDULE);
}

static void apply_relay(ControlManager_t *mgr, uint8_t id, RelayState_t state)
{
    /* Use relay_manager_request (hysteresis will handle) */
    (void)relay_manager_request(mgr->config.relay_mgr, id, state);
}

/* ============================================================================
 *   PUBLIC API
 * ============================================================================ */

ErrorCode_t control_manager_init(ControlManager_t             *mgr,
                                  const ControlManagerConfig_t *config,
                                  ControlMode_t                 initial_mode)
{
    if ((mgr == NULL) || (config == NULL) ||
        (config->sensor_mgr == NULL) ||
        (config->relay_mgr == NULL) ||
        (config->schedule_mgr == NULL) ||
        (config->rtc_mgr == NULL))
    {
        return ERR_INVALID_PARAM;
    }

    memcpy(&mgr->config, config, sizeof(ControlManagerConfig_t));
    mgr->mode = initial_mode;
    mgr->last_update_ms = 0U;
    mgr->heat.active = false;
    mgr->heat.phase_end_ms = 0U;
    mgr->heat.in_on_phase = false;
    mgr->initialized = true;

    debug_log("control_manager: init with mode %d\r\n", initial_mode);
    return ERR_NONE;
}

void control_manager_update(ControlManager_t *mgr)
{
    if ((mgr == NULL) || (!mgr->initialized))
    {
        return;
    }

    /* Rate limit: only run every SENSOR_READ_INTERVAL_MS */
    uint32_t now = timing_get_ms();
    if ((now - mgr->last_update_ms) < SENSOR_READ_INTERVAL_MS)
    {
        return;
    }
    mgr->last_update_ms = now;

    /* Track whether we already started a heat-protection pulse cycle */
    static bool heat_was_active = false;

    /* --- Read sensors --- */
    const SensorData_t *sensor_data;
    if (sensor_manager_get_data(mgr->config.sensor_mgr, &sensor_data) != ERR_NONE)
    {
        debug_log("control: cannot get sensor data\r\n");
        return;
    }

    /* --- Read current time --- */
    TimeOfDay_t now_time;
    if (rtc_manager_get_time(mgr->config.rtc_mgr, &now_time) != ERR_NONE)
    {
        debug_log("control: cannot get RTC time\r\n");
        return;
    }

    /* --- Decide pump and mist states based on mode --- */
    RelayState_t pump_req = RELAY_OFF;
    RelayState_t mist_req = RELAY_OFF;

    if (mgr->mode == MODE_MANUAL)
    {
        /* Manual: do not override; leave relays as they are */
        return;
    }

    /* ----- Heat protection override (highest priority) ----- */
    bool heat_active = false;
    if ((sensor_data->temperature > HEAT_TEMP_THRESHOLD_C) &&
        (sensor_data->light_lux > HEAT_LIGHT_THRESHOLD_LUX) &&
        (sensor_data->humidity < HUMIDITY_LOW_THRESHOLD))
    {
        heat_active = true;
    }

    if (heat_active)
    {
        /* Mark heat-protection active */
        mgr->heat.active = true;

        /* Manage pulse timing via relay_manager_pulse */
        if (!heat_was_active)
        {
            /* Start heat pulse: ON for HEAT_MIST_PULSE_ON_MS, OFF for HEAT_MIST_PULSE_OFF_MS */
            (void)relay_manager_pulse(mgr->config.relay_mgr, RELAY_ID_MIST,
                                      HEAT_MIST_PULSE_ON_MS, HEAT_MIST_PULSE_OFF_MS);
            heat_was_active = true;
        }

        /* For safety, turn pump off during heat */
        pump_req = RELAY_OFF;
        /* Mist will be handled by pulse, so we don't set mist_req here */
    }
    else
    {
        /* Reset heat state */
        mgr->heat.active = false;
        heat_was_active = false;
        /* Cancel any ongoing pulse */
        relay_manager_request(mgr->config.relay_mgr, RELAY_ID_MIST, RELAY_OFF);
    }

    /* ----- Pump logic (if not heat) ----- */
    if (!heat_active)
    {
        if (mgr->mode == MODE_AUTO)
        {
            /* Auto: compare soil moisture */
            if (sensor_data->soil_moisture < SOIL_DRY_THRESHOLD)
            {
                pump_req = RELAY_ON;
            }
            else if (sensor_data->soil_moisture > SOIL_WET_THRESHOLD)
            {
                pump_req = RELAY_OFF;
            }
            /* else keep previous state (we read applied state later) */
            /* But we have to get current state to hold */
            RelayState_t current_pump;
            relay_manager_get_state(mgr->config.relay_mgr, RELAY_ID_PUMP, &current_pump);
            if ((sensor_data->soil_moisture >= SOIL_DRY_THRESHOLD) &&
                (sensor_data->soil_moisture <= SOIL_WET_THRESHOLD))
            {
                pump_req = current_pump; /* maintain */
            }
        }
        else if (mgr->mode == MODE_SCHEDULE)
        {
            /* Schedule: check schedule manager */
            DateOfDay_t today;
            rtc_manager_get_date(mgr->config.rtc_mgr, &today);
            if (schedule_manager_should_water(mgr->config.schedule_mgr, &now_time, &today))
            {
                pump_req = RELAY_ON;
            }
            else
            {
                pump_req = RELAY_OFF;
            }
        }

        /* ----- Forbidden period override (non-manual) ----- */
        if (rtc_manager_is_forbidden(mgr->config.rtc_mgr))
        {
            pump_req = RELAY_OFF;
        }
    }

    /* ----- Mist logic (if not in heat) ----- */
    if (!heat_active)
    {
        if (mgr->mode == MODE_AUTO || mgr->mode == MODE_SCHEDULE)
        {
            if (sensor_data->humidity < HUMIDITY_LOW_THRESHOLD)
            {
                mist_req = RELAY_ON;
            }
            else if (sensor_data->humidity > HUMIDITY_HIGH_THRESHOLD)
            {
                mist_req = RELAY_OFF;
            }
            else
            {
                RelayState_t current_mist;
                relay_manager_get_state(mgr->config.relay_mgr, RELAY_ID_MIST, &current_mist);
                mist_req = current_mist;
            }
        }
    }

    /* ---- Apply requests ---- */
    apply_relay(mgr, RELAY_ID_PUMP, pump_req);
    if (!heat_active)
    {
        apply_relay(mgr, RELAY_ID_MIST, mist_req);
    }
    /* If heat_active, mist is controlled by pulse, so we don't override */
}

ErrorCode_t control_manager_set_mode(ControlManager_t *mgr,
                                      ControlMode_t     mode)
{
    if ((mgr == NULL) || (!mgr->initialized))
    {
        return ERR_INVALID_PARAM;
    }
    mgr->mode = mode;
    debug_log("control: mode changed to %d\r\n", mode);
    return ERR_NONE;
}

ErrorCode_t control_manager_get_mode(const ControlManager_t *mgr,
                                      ControlMode_t          *mode_out)
{
    if ((mgr == NULL) || (mode_out == NULL) || (!mgr->initialized))
    {
        return ERR_INVALID_PARAM;
    }
    *mode_out = mgr->mode;
    return ERR_NONE;
}

ErrorCode_t control_manager_manual_set(ControlManager_t *mgr,
                                        uint8_t           relay_id,
                                        RelayState_t      state)
{
    if ((mgr == NULL) || (!mgr->initialized) || (mgr->mode != MODE_MANUAL))
    {
        return ERR_INVALID_PARAM;
    }
    return relay_manager_request(mgr->config.relay_mgr, relay_id, state);
}

bool control_manager_is_heat_protect_active(const ControlManager_t *mgr)
{
    if ((mgr == NULL) || (!mgr->initialized))
    {
        return false;
    }
    return mgr->heat.active;
}

void control_manager_emergency_stop(ControlManager_t *mgr)
{
    if ((mgr == NULL) || (!mgr->initialized))
    {
        return;
    }
    relay_manager_emergency_off(mgr->config.relay_mgr);
    mgr->mode = MODE_MANUAL;
    debug_log("control: emergency stop\r\n");
}
