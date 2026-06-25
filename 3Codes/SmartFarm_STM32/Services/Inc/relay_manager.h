/**
 * @file    relay_manager.h
 * @brief   Relay management service – adds hysteresis, pulse-timing,
 *          and mode-awareness on top of the raw relay driver.
 * @author  ltc1342
 * @date    2026-07-01
 *
 * @note    Hysteresis prevents rapid toggling: a relay cannot change
 *          state again until RELAY_HYSTERESIS_MS (app_defs.h) has
 *          elapsed since the last state change.
 *
 *          Heat-protection pulse mode: mist can be activated for a
 *          fixed ON duration then forced OFF for a cool-down period
 *          (HEAT_MIST_PULSE_ON_MS / HEAT_MIST_PULSE_OFF_MS in app_defs.h).
 *
 *          relay_manager_update() must be called periodically (e.g. every
 *          100 ms from the main loop) to process timed state transitions.
 *
 *          Usage:
 *          @code
 *            static RelayManager_t relay_mgr;
 *            relay_manager_init(&relay_mgr, &g_relay_handle, 0U);
 *
 *            // Request pump ON; hysteresis enforced internally
 *            relay_manager_request(&relay_mgr, RELAY_ID_PUMP, RELAY_ON);
 *
 *            // In main loop (every ~100 ms)
 *            relay_manager_update(&relay_mgr);
 *          @endcode
 *
 * @changelog
 *   2026-07-01  ltc1342   Added pulse_off_ms to RelayChannel_t to support
 *                         two-phase (ON → OFF) pulse FSM in relay_manager_pulse().
 */

#ifndef RELAY_MANAGER_H_
#define RELAY_MANAGER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "app_types.h"
#include "app_defs.h"
#include "relay.h"

/* ============================================================================
 *   TYPES
 * ============================================================================ */

/**
 * @brief  Per-relay runtime state tracked by the manager.
 */
typedef struct {
    RelayState_t requested_state;   /**< Latest requested state                  */
    RelayState_t applied_state;     /**< State currently on the GPIO             */
    uint32_t     last_change_ms;    /**< HAL_GetTick() timestamp of last change  */
    bool         pulse_active;      /**< True while in timed pulse mode          */
    uint32_t     pulse_end_ms;      /**< HAL_GetTick() when current phase ends   */
    uint32_t     pulse_off_ms;      /**< Duration [ms] of the OFF cool-down phase
                                     *   Set by relay_manager_pulse(); consumed
                                     *   in relay_manager_update() when the ON
                                     *   phase completes.                         */
} RelayChannel_t;

/**
 * @brief  Relay manager instance.
 */
typedef struct {
    RelayHandle_t  *relay;                   /**< Underlying driver (injected)    */
    RelayChannel_t  channel[RELAY_ID_COUNT]; /**< Per-relay runtime state         */
    uint32_t        hysteresis_ms;           /**< Min ms between state changes    */
    bool            initialized;
} RelayManager_t;

/* ============================================================================
 *   API
 * ============================================================================ */

/**
 * @brief  Initialise relay manager.
 * @param  mgr            Manager instance (must not be NULL).
 * @param  relay          Initialised relay driver handle (must not be NULL).
 * @param  hysteresis_ms  Minimum time between state changes per relay [ms].
 *                        Pass 0 to use RELAY_HYSTERESIS_MS from app_defs.h.
 * @return ERR_NONE on success, ERR_INVALID_PARAM on NULL.
 */
ErrorCode_t relay_manager_init(RelayManager_t *mgr,
                                RelayHandle_t  *relay,
                                uint32_t        hysteresis_ms);

/**
 * @brief  Request a logical state change for a relay.
 * @note   The change is applied immediately if the hysteresis window has
 *         elapsed; otherwise the request is stored and re-evaluated on the
 *         next relay_manager_update() call.  An explicit request cancels
 *         any active pulse on that channel.
 * @param  mgr       Initialised manager.
 * @param  relay_id  RELAY_ID_PUMP or RELAY_ID_MIST.
 * @param  state     RELAY_ON or RELAY_OFF.
 * @return ERR_NONE on success.
 */
ErrorCode_t relay_manager_request(RelayManager_t *mgr,
                                   uint8_t         relay_id,
                                   RelayState_t    state);

/**
 * @brief  Start a timed ON pulse for a relay (heat-protection mode).
 * @param  mgr         Initialised manager.
 * @param  relay_id    RELAY_ID_PUMP or RELAY_ID_MIST.
 * @param  on_ms       ON duration [ms].
 * @param  off_ms      OFF (cool-down) duration [ms] after ON finishes.
 * @return ERR_NONE on success.
 * @note   relay_manager_update() drives the pulse timing.
 *         Hysteresis is bypassed for pulse transitions (the pulse FSM
 *         owns the relay for the duration of both phases).
 */
ErrorCode_t relay_manager_pulse(RelayManager_t *mgr,
                                 uint8_t         relay_id,
                                 uint32_t        on_ms,
                                 uint32_t        off_ms);

/**
 * @brief  Process timed transitions – call every ~100 ms from main loop.
 * @param  mgr  Initialised manager.
 * @note   Handles: pending requests blocked by hysteresis, pulse phase
 *         transitions (ON → OFF → idle).
 */
void relay_manager_update(RelayManager_t *mgr);

/**
 * @brief  Read the currently applied (physical) state of a relay.
 * @param  mgr        Initialised manager.
 * @param  relay_id   RELAY_ID_PUMP or RELAY_ID_MIST.
 * @param  state_out  Destination (must not be NULL).
 * @return ERR_NONE on success.
 */
ErrorCode_t relay_manager_get_state(const RelayManager_t *mgr,
                                     uint8_t               relay_id,
                                     RelayState_t         *state_out);

/**
 * @brief  Fill a RelayStatus_t snapshot with both relay states.
 * @param  mgr     Initialised manager.
 * @param  status  Destination structure (must not be NULL).
 * @return ERR_NONE on success.
 */
ErrorCode_t relay_manager_get_status(const RelayManager_t *mgr,
                                      RelayStatus_t        *status);

/**
 * @brief  Immediately force all relays OFF and cancel any pending pulse.
 * @param  mgr  Initialised manager (silently ignored if NULL).
 */
void relay_manager_emergency_off(RelayManager_t *mgr);

/**
 * @brief  Check whether a relay's hysteresis window is still open.
 * @param  mgr       Initialised manager.
 * @param  relay_id  RELAY_ID_PUMP or RELAY_ID_MIST.
 * @return true if the relay cannot yet be toggled (cooldown still active).
 */
bool relay_manager_is_locked(const RelayManager_t *mgr, uint8_t relay_id);

#ifdef __cplusplus
}
#endif

#endif /* RELAY_MANAGER_H_ */
