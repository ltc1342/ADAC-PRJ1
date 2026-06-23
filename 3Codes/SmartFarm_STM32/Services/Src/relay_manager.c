/**
 * @file    relay_manager.c
 * @brief   Relay management service with hysteresis and pulse timing.
 * @author  Group SmartFarm
 * @date    2026-07-01
 */

#include "relay_manager.h"
#include "timing.h"
#include "debug_log.h"
#include <string.h>

/* ============================================================================
 *   PRIVATE HELPERS
 * ============================================================================ */

static bool is_relay_id_valid(uint8_t id)
{
    return (id == RELAY_ID_PUMP) || (id == RELAY_ID_MIST);
}

static void apply_state(RelayManager_t *mgr, uint8_t id, RelayState_t state)
{
    RelayChannel_t *ch = &mgr->channel[id];
    if (ch->applied_state != state)
    {
        (void)relay_set(mgr->relay, id, state);
        ch->applied_state = state;
        ch->last_change_ms = timing_get_ms();
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

    mgr->relay = relay;
    mgr->hysteresis_ms = (hysteresis_ms == 0U) ? RELAY_HYSTERESIS_MS : hysteresis_ms;

    /* Initialise channels */
    for (uint8_t i = 0U; i < RELAY_ID_COUNT; i++)
    {
        mgr->channel[i].requested_state = RELAY_OFF;
        mgr->channel[i].applied_state   = RELAY_OFF;
        mgr->channel[i].last_change_ms  = 0U;
        mgr->channel[i].pulse_active    = false;
        mgr->channel[i].pulse_end_ms    = 0U;
    }

    mgr->initialized = true;

    /* Force all relays off initially */
    relay_all_off(relay);
    debug_log("relay_manager: init OK\r\n");
    return ERR_NONE;
}

ErrorCode_t relay_manager_request(RelayManager_t *mgr,
                                   uint8_t         relay_id,
                                   RelayState_t    state)
{
    if ((mgr == NULL) || (!mgr->initialized) || (!is_relay_id_valid(relay_id)))
    {
        return ERR_INVALID_PARAM;
    }

    mgr->channel[relay_id].requested_state = state;
    return ERR_NONE;
}

ErrorCode_t relay_manager_pulse(RelayManager_t *mgr,
                                 uint8_t         relay_id,
                                 uint32_t        on_ms,
                                 uint32_t        off_ms)
{
    if ((mgr == NULL) || (!mgr->initialized) || (!is_relay_id_valid(relay_id)))
    {
        return ERR_INVALID_PARAM;
    }

    RelayChannel_t *ch = &mgr->channel[relay_id];
    /* Cancel any existing pulse */
    ch->pulse_active = false;

    /* Start ON phase */
    ch->pulse_active = true;
    ch->pulse_end_ms = timing_get_ms() + on_ms;
    ch->requested_state = RELAY_ON;   /* will be applied in update */

    return ERR_NONE;
}

void relay_manager_update(RelayManager_t *mgr)
{
    if ((mgr == NULL) || (!mgr->initialized))
    {
        return;
    }

    uint32_t now = timing_get_ms();

    for (uint8_t id = 0U; id < RELAY_ID_COUNT; id++)
    {
        RelayChannel_t *ch = &mgr->channel[id];

        /* --- Pulse handling --- */
        if (ch->pulse_active)
        {
            if (now >= ch->pulse_end_ms)
            {
                /* End of current phase, toggle to OFF */
                ch->pulse_active = false;
                ch->requested_state = RELAY_OFF;
                /* Force apply immediately (no hysteresis) */
                apply_state(mgr, id, RELAY_OFF);
                continue; /* skip normal hysteresis for this cycle */
            }
            /* Still in ON phase, ensure relay is ON */
            apply_state(mgr, id, RELAY_ON);
            continue;
        }

        /* --- Normal state change with hysteresis --- */
        if (ch->applied_state == ch->requested_state)
        {
            continue; /* no change needed */
        }

        uint32_t elapsed = now - ch->last_change_ms;
        if (elapsed >= mgr->hysteresis_ms)
        {
            apply_state(mgr, id, ch->requested_state);
        }
    }
}

ErrorCode_t relay_manager_get_state(const RelayManager_t *mgr,
                                     uint8_t               relay_id,
                                     RelayState_t         *state_out)
{
    if ((mgr == NULL) || (state_out == NULL) || (!mgr->initialized) ||
        (!is_relay_id_valid(relay_id)))
    {
        return ERR_INVALID_PARAM;
    }

    *state_out = mgr->channel[relay_id].applied_state;
    return ERR_NONE;
}

ErrorCode_t relay_manager_get_status(const RelayManager_t *mgr,
                                      RelayStatus_t        *status)
{
    if ((mgr == NULL) || (status == NULL) || (!mgr->initialized))
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

    for (uint8_t id = 0U; id < RELAY_ID_COUNT; id++)
    {
        mgr->channel[id].requested_state = RELAY_OFF;
        mgr->channel[id].pulse_active = false;
        apply_state(mgr, id, RELAY_OFF);
    }
    debug_log("relay_manager: emergency off\r\n");
}

bool relay_manager_is_locked(const RelayManager_t *mgr, uint8_t relay_id)
{
    if ((mgr == NULL) || (!mgr->initialized) || (!is_relay_id_valid(relay_id)))
    {
        return false;
    }

    uint32_t now = timing_get_ms();
    uint32_t elapsed = now - mgr->channel[relay_id].last_change_ms;
    return (elapsed < mgr->hysteresis_ms);
}
