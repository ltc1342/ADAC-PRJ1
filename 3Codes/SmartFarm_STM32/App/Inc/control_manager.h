/**
 * @file    control_manager.h
 * @brief   Automatic control engine – threshold-based + schedule-based
 *          actuation of pump and mist relays.
 * @author  ltc1342
 * @date    2026-07-01
 *
 * @note    Modes (ControlMode_t, defined in app_types.h):
 *
 *          MODE_AUTO     – threshold comparison drives relays:
 *            Pump  ON  when soil moisture < SOIL_DRY_THRESHOLD (40 %)
 *            Pump  OFF when soil moisture > SOIL_WET_THRESHOLD (70 %)
 *            Mist  ON  when humidity     < HUMIDITY_LOW_THRESHOLD (50 %)
 *            Mist  OFF when humidity     > HUMIDITY_HIGH_THRESHOLD (70 %)
 *            Heat-protection override when:
 *              temp > HEAT_TEMP_THRESHOLD_C  AND
 *              light > HEAT_LIGHT_THRESHOLD_LUX AND
 *              humidity < HUMIDITY_LOW_THRESHOLD
 *
 *          MODE_SCHEDULE – pump follows the weekly schedule table;
 *                          mist follows humidity thresholds.
 *
 *          MODE_MANUAL   – control_manager ignores sensors; caller
 *                          drives relays via control_manager_manual_set().
 *
 *          Forbidden watering period (10:00–15:00) is enforced in all
 *          non-manual modes.
 *
 *          Usage:
 *          @code
 *            static ControlManager_t ctrl_mgr;
 *            ControlManagerConfig_t  cfg = {
 *                .sensor_mgr  = &g_sensor_mgr,
 *                .relay_mgr   = &g_relay_mgr,
 *                .schedule_mgr = &g_sched_mgr,
 *                .rtc_mgr     = &g_rtc_mgr,
 *            };
 *            control_manager_init(&ctrl_mgr, &cfg, MODE_AUTO);
 *
 *            // In main loop (call every SENSOR_READ_INTERVAL_MS):
 *            control_manager_update(&ctrl_mgr);
 *          @endcode
 */

#ifndef CONTROL_MANAGER_H_
#define CONTROL_MANAGER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "app_types.h"
#include "app_defs.h"
#include "sensor_manager.h"
#include "relay_manager.h"
#include "schedule_manager.h"
#include "rtc_manager.h"

/* ============================================================================
 *   TYPES
 * ============================================================================ */

/**
 * @brief  Service references injected at initialisation.
 */
typedef struct {
    SensorManager_t   *sensor_mgr;     /**< Must not be NULL */
    RelayManager_t    *relay_mgr;      /**< Must not be NULL */
    ScheduleManager_t *schedule_mgr;   /**< Must not be NULL */
    RtcManager_t      *rtc_mgr;        /**< Must not be NULL */
} ControlManagerConfig_t;

/**
 * @brief  Heat-protection runtime state.
 */
typedef struct {
    bool     active;            /**< True while heat-protection engaged  */
    uint32_t phase_end_ms;      /**< End of current ON/OFF phase         */
    bool     in_on_phase;       /**< True = mist ON, false = cool-down   */
} HeatProtectionState_t;

/**
 * @brief  Control manager instance.
 */
typedef struct {
    ControlManagerConfig_t  config;
    ControlMode_t           mode;
    HeatProtectionState_t   heat;
    uint32_t                last_update_ms;    /**< For rate-limiting update() */
    bool                    initialized;
} ControlManager_t;

/* ============================================================================
 *   API
 * ============================================================================ */

/**
 * @brief  Initialise control manager.
 * @param  mgr           Manager instance (must not be NULL).
 * @param  config        Service references (all fields must be non-NULL).
 * @param  initial_mode  Starting mode (MODE_AUTO recommended).
 * @return ERR_NONE on success, ERR_INVALID_PARAM on NULL or missing refs.
 */
ErrorCode_t control_manager_init(ControlManager_t             *mgr,
                                  const ControlManagerConfig_t *config,
                                  ControlMode_t                 initial_mode);

/**
 * @brief  Run one control cycle (read sensors → evaluate → actuate).
 * @param  mgr  Initialised manager.
 * @note   Call every SENSOR_READ_INTERVAL_MS from the main loop.
 *         Internally rate-limited; safe to call more frequently.
 */
void control_manager_update(ControlManager_t *mgr);

/**
 * @brief  Switch operating mode at runtime (e.g. via MQTT command).
 * @param  mgr   Initialised manager.
 * @param  mode  New mode (MODE_AUTO / MODE_MANUAL / MODE_SCHEDULE).
 * @return ERR_NONE on success.
 * @note   Switching to MODE_MANUAL does NOT change relay states;
 *         call control_manager_manual_set() to command relays explicitly.
 */
ErrorCode_t control_manager_set_mode(ControlManager_t *mgr,
                                      ControlMode_t     mode);

/**
 * @brief  Read the current operating mode.
 * @param  mgr      Initialised manager.
 * @param  mode_out Destination (must not be NULL).
 * @return ERR_NONE on success.
 */
ErrorCode_t control_manager_get_mode(const ControlManager_t *mgr,
                                      ControlMode_t          *mode_out);

/**
 * @brief  Manually command a relay (valid in MODE_MANUAL only).
 * @param  mgr       Initialised manager.
 * @param  relay_id  RELAY_ID_PUMP or RELAY_ID_MIST.
 * @param  state     RELAY_ON or RELAY_OFF.
 * @return ERR_NONE on success, ERR_INVALID_PARAM if not in MODE_MANUAL.
 */
ErrorCode_t control_manager_manual_set(ControlManager_t *mgr,
                                        uint8_t           relay_id,
                                        RelayState_t      state);

/**
 * @brief  Check whether heat-protection mode is currently active.
 * @param  mgr  Initialised manager.
 * @return true if heat-protection override is engaging the mist relay.
 */
bool control_manager_is_heat_protect_active(const ControlManager_t *mgr);

/**
 * @brief  Force a safe state: all relays OFF, mode → MODE_MANUAL.
 * @param  mgr  Initialised manager (silently ignored if NULL).
 * @note   Call on critical fault or watchdog trigger.
 */
void control_manager_emergency_stop(ControlManager_t *mgr);

#ifdef __cplusplus
}
#endif

#endif /* CONTROL_MANAGER_H_ */
