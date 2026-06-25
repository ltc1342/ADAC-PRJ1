/**
 * @file    relay_manager.c
 * @brief   Relay management service implementation.
 *
 * @details Implements three layers of relay control on top of the raw
 *          relay driver:
 *
 *          1. Hysteresis – a relay cannot change state until at least
 *             hysteresis_ms milliseconds have elapsed since the last
 *             change. Pending requests are re-evaluated on every
 *             relay_manager_update() call.
 *
 *          2. Pulse mode (two-phase FSM) – relay_manager_pulse() starts
 *             an ON phase of on_ms duration, then automatically switches
 *             to an OFF phase of off_ms duration.  The pulse FSM bypasses
 *             hysteresis so that both phase transitions are applied on time.
 *
 *          3. Emergency off – immediately forces all relays OFF regardless
 *             of hysteresis or pulse state (fault / safety path).
 *
 * @author  ltc1342
 * @date    2026-07-01
 *
 * @note    All timestamps use HAL_GetTick() (32-bit ms counter, wraps every
 *          ~49.7 days) instead of timing_get_ms() (DWT-based, wraps every
 *          ~44.7 s at 96 MHz).  This is critical for the heat-protection
 *          pulse OFF phase which can be up to 300,000 ms (5 minutes).
 */

#include "relay_manager.h"
#include "debug_log.h"
#include "stm32f4xx_hal.h"   /* HAL_GetTick() */

/* ============================================================================
 *   PRIVATE HELPERS
 * ============================================================================ */

/**
 * @brief  Write a state change directly to the GPIO layer and update
 *         the channel bookkeeping.  Always succeeds (no hysteresis check).
 * @param  mgr       Initialised manager.
 * @param  relay_id  Channel index (caller guarantees < RELAY_ID_COUNT).
 * @param  state     Target logical state.
 */
static void force_apply_state(RelayManager_t *mgr,
                               uint8_t         relay_id,
                               RelayState_t    state)
{
    RelayChannel_t *ch = &mgr->channel[relay_id];

    (void)relay_set(mgr->relay, relay_id, state);

    ch->applied_state  = state;
    ch->last_change_ms = HAL_GetTick();
}

/**
 * @brief  Attempt to apply a state change, enforcing the hysteresis window.
 * @param  mgr       Initialised manager.
 * @param  relay_id  Channel index (caller guarantees < RELAY_ID_COUNT).
 * @param  state     Requested logical state.
 * @return true  – state was applied to the GPIO.
 *         false – still inside the hysteresis cooldown; try again later.
 */
static bool try_apply_state(RelayManager_t *mgr,
                             uint8_t         relay_id,
                             RelayState_t    state)
{
    const RelayChannel_t *ch  = &mgr->channel[relay_id];
    uint32_t              now = HAL_GetTick();

    /* Already in the requested state – nothing to do */
    if (ch->applied_state == state)
    {
        return true;
    }

    /* Unsigned subtraction handles 32-bit wrap correctly */
    if ((now - ch->last_change_ms) < mgr->hysteresis_ms)
    {
        return false;   /* Still inside the cooldown window */
    }

    force_apply_state(mgr, relay_id, state);
    return true;
}

/**
 * @brief  Drive the two-phase pulse FSM for a single channel.
 * @param  mgr       Initialised manager.
 * @param  relay_id  Channel index (caller guarantees < RELAY_ID_COUNT).
 */
static void update_pulse_channel(RelayManager_t *mgr, uint8_t relay_id)
{
    RelayChannel_t *ch  = &mgr->channel[relay_id];
    uint32_t        now = HAL_GetTick();

    /* Check whether the current phase (ON or OFF) has expired */
    if ((now - ch->pulse_end_ms) > UINT32_MAX / 2U)
    {
        /* Subtraction would wrap negatively – phase hasn't ended yet.
         * This guard handles the case where now < pulse_end_ms correctly
         * for unsigned arithmetic. */
        return;
    }

    if (ch->applied_state == RELAY_ON)
    {
        /* ON phase expired → start the OFF cool-down phase */
        debug_log("[RelayMgr] relay[%u] pulse ON→OFF (cool-down %lu ms)\r\n",
                  (unsigned)relay_id, (unsigned long)ch->pulse_off_ms);

        force_apply_state(mgr, relay_id, RELAY_OFF);

        /* Arm the end of the OFF phase */
        ch->pulse_end_ms = HAL_GetTick() + ch->pulse_off_ms;
    }
    else
    {
        /* OFF (cool-down) phase expired → pulse complete */
        debug_log("[RelayMgr] relay[%u] pulse complete, returning to idle\r\n",
                  (unsigned)relay_id);

        ch->pulse_active    = false;
        ch->requested_state = RELAY_OFF;
        /* Relay is already OFF – no GPIO write needed */
    }
}

/* ============================================================================
 *   PUBLIC API
 * ============================================================================ */

ErrorCode_t relay_manager_init(RelayManager_t *mgr,
                                RelayHandle_t  *relay,
                                uint32_t        hysteresis_ms)
{
    if ((mgr == NULL) || (relay == NULL))
    {
        return ERR_INVALID_PARAM;
    }

    mgr->relay       = relay;
    mgr->initialized = true;

    /* Fall back to the compile-time default when caller passes 0 */
    mgr->hysteresis_ms = (hysteresis_ms == 0U) ? RELAY_HYSTERESIS_MS
                                                 : hysteresis_ms;

    /* All channels start OFF, no pending pulse, cooldown timer at epoch */
    for (uint8_t i = 0U; i < RELAY_ID_COUNT; i++)
    {
        mgr->channel[i].requested_state = RELAY_OFF;
        mgr->channel[i].applied_state   = RELAY_OFF;
        mgr->channel[i].last_change_ms  = 0U;
        mgr->channel[i].pulse_active    = false;
        mgr->channel[i].pulse_end_ms    = 0U;
        mgr->channel[i].pulse_off_ms    = 0U;
    }

    debug_log("[RelayMgr] initialised, hysteresis=%lu ms\r\n",
              (unsigned long)mgr->hysteresis_ms);

    return ERR_NONE;
}

ErrorCode_t relay_manager_request(RelayManager_t *mgr,
                                   uint8_t         relay_id,
                                   RelayState_t    state)
{
    if ((mgr == NULL) || (!mgr->initialized) || (relay_id >= RELAY_ID_COUNT))
    {
        return ERR_INVALID_PARAM;
    }

    RelayChannel_t *ch = &mgr->channel[relay_id];

    /* An explicit request cancels any running pulse so the caller regains
     * control of the relay immediately */
    if (ch->pulse_active)
    {
        debug_log("[RelayMgr] relay[%u] pulse cancelled by explicit request\r\n",
                  (unsigned)relay_id);
        ch->pulse_active = false;
    }

    ch->requested_state = state;

    /* Attempt to apply now; if locked by hysteresis, update() will retry */
    if (!try_apply_state(mgr, relay_id, state))
    {
        debug_log("[RelayMgr] relay[%u] request queued (hysteresis lock)\r\n",
                  (unsigned)relay_id);
    }

    return ERR_NONE;
}

ErrorCode_t relay_manager_pulse(RelayManager_t *mgr,
                                 uint8_t         relay_id,
                                 uint32_t        on_ms,
                                 uint32_t        off_ms)
{
    if ((mgr == NULL) || (!mgr->initialized) || (relay_id >= RELAY_ID_COUNT))
    {
        return ERR_INVALID_PARAM;
    }

    if ((on_ms == 0U) || (off_ms == 0U))
    {
        return ERR_INVALID_PARAM;
    }

    RelayChannel_t *ch = &mgr->channel[relay_id];

    debug_log("[RelayMgr] relay[%u] pulse ON %lu ms / OFF %lu ms\r\n",
              (unsigned)relay_id,
              (unsigned long)on_ms,
              (unsigned long)off_ms);

    /* Pulse bypasses hysteresis – force relay ON immediately */
    force_apply_state(mgr, relay_id, RELAY_ON);

    ch->requested_state = RELAY_ON;
    ch->pulse_active    = true;
    ch->pulse_end_ms    = HAL_GetTick() + on_ms;
    ch->pulse_off_ms    = off_ms;

    return ERR_NONE;
}

void relay_manager_update(RelayManager_t *mgr)
{
    if ((mgr == NULL) || (!mgr->initialized))
    {
        return;
    }

    for (uint8_t i = 0U; i < RELAY_ID_COUNT; i++)
    {
        RelayChannel_t *ch = &mgr->channel[i];

        if (ch->pulse_active)
        {
            /* Pulse FSM owns the channel – drive phase transitions */
            update_pulse_channel(mgr, i);
        }
        else if (ch->requested_state != ch->applied_state)
        {
            /* Pending request: retry if the hysteresis window has closed */
            (void)try_apply_state(mgr, i, ch->requested_state);
        }
        else
        {
            /* Nothing to do for this channel */
        }
    }
}

ErrorCode_t relay_manager_get_state(const RelayManager_t *mgr,
                                     uint8_t               relay_id,
                                     RelayState_t         *state_out)
{
    if ((mgr == NULL) || (!mgr->initialized) ||
        (relay_id >= RELAY_ID_COUNT) || (state_out == NULL))
    {
        return ERR_INVALID_PARAM;
    }

    *state_out = mgr->channel[relay_id].applied_state;
    return ERR_NONE;
}

ErrorCode_t relay_manager_get_status(const RelayManager_t *mgr,
                                      RelayStatus_t        *status)
{
    if ((mgr == NULL) || (!mgr->initialized) || (status == NULL))
    {
        return ERR_INVALID_PARAM;
    }

    status->pump = mgr->channel[RELAY_ID_PUMP].applied_state;
    status->mist = mgr->channel[RELAY_ID_MIST].applied_state;
    return ERR_NONE;
}

void relay_manager_emergency_off(RelayManager_t *mgr)
{
    if ((mgr == NULL) || (!mgr->initialized))
    {
        return;
    }

    debug_log("[RelayMgr] EMERGENCY OFF – all relays forced OFF\r\n");

    /* Force all relays OFF immediately, bypassing hysteresis */
    for (uint8_t i = 0U; i < RELAY_ID_COUNT; i++)
    {
        RelayChannel_t *ch = &mgr->channel[i];

        ch->pulse_active    = false;
        ch->pulse_end_ms    = 0U;
        ch->pulse_off_ms    = 0U;
        ch->requested_state = RELAY_OFF;

        force_apply_state(mgr, i, RELAY_OFF);
    }
}

bool relay_manager_is_locked(const RelayManager_t *mgr, uint8_t relay_id)
{
    if ((mgr == NULL) || (!mgr->initialized) || (relay_id >= RELAY_ID_COUNT))
    {
        return false;
    }

    uint32_t elapsed = HAL_GetTick() - mgr->channel[relay_id].last_change_ms;
    return (elapsed < mgr->hysteresis_ms);
}
