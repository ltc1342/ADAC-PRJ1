/**
 * @file    relay.c
 * @brief   Relay driver implementation (Active-LOW, LL GPIO).
 * @author  ltc1342
 * @date    2026-07-01
 */

#include "relay.h"
#include "stm32f4xx_ll_gpio.h"

/* ============================================================================
 *   PRIVATE HELPERS
 * ============================================================================ */

/**
 * @brief  Apply a logical state directly to GPIO.
 * @note   Active-LOW: RELAY_ON  → ResetOutputPin (GPIO LOW → coil ON).
 *                     RELAY_OFF → SetOutputPin   (GPIO HIGH → coil OFF).
 */
static void apply_gpio_state(const RelayPin_t *pin, RelayState_t state)
{
    if (state == RELAY_ON) {
        LL_GPIO_ResetOutputPin(pin->port, pin->pin);
    } else {
        LL_GPIO_SetOutputPin(pin->port, pin->pin);
    }
}

/**
 * @brief  Return true if relay_id is within valid range.
 */
static bool is_valid_id(uint8_t relay_id)
{
    return (relay_id < RELAY_ID_COUNT);
}

/* ============================================================================
 *   PUBLIC API
 * ============================================================================ */

ErrorCode_t relay_init(RelayHandle_t *handle,
                       RelayPin_t     pump,
                       RelayPin_t     mist)
{
    if (handle == NULL) {
        return ERR_INVALID_PARAM;
    }

    handle->pin_cfg[RELAY_ID_PUMP] = pump;
    handle->pin_cfg[RELAY_ID_MIST] = mist;
    handle->state[RELAY_ID_PUMP]   = RELAY_OFF;
    handle->state[RELAY_ID_MIST]   = RELAY_OFF;
    handle->initialized            = true;

    /* Drive GPIO to safe state immediately (HIGH = coil OFF for active-LOW) */
    LL_GPIO_SetOutputPin(pump.port, pump.pin);
    LL_GPIO_SetOutputPin(mist.port, mist.pin);

    return ERR_NONE;
}

ErrorCode_t relay_set(RelayHandle_t *handle,
                      uint8_t        relay_id,
                      RelayState_t   state)
{
    if ((handle == NULL) || (!handle->initialized) || (!is_valid_id(relay_id))) {
        return ERR_INVALID_PARAM;
    }

    handle->state[relay_id] = state;
    apply_gpio_state(&handle->pin_cfg[relay_id], state);

    return ERR_NONE;
}

ErrorCode_t relay_get_state(const RelayHandle_t *handle,
                             uint8_t              relay_id,
                             RelayState_t        *state_out)
{
    if ((handle == NULL) || (state_out == NULL) || (!is_valid_id(relay_id))) {
        return ERR_INVALID_PARAM;
    }

    *state_out = handle->state[relay_id];
    return ERR_NONE;
}

ErrorCode_t relay_toggle(RelayHandle_t *handle, uint8_t relay_id)
{
    RelayState_t current;
    ErrorCode_t  err;

    err = relay_get_state(handle, relay_id, &current);
    if (err != ERR_NONE) {
        return err;
    }

    return relay_set(handle, relay_id,
                     (current == RELAY_ON) ? RELAY_OFF : RELAY_ON);
}

void relay_all_off(RelayHandle_t *handle)
{
    if ((handle == NULL) || (!handle->initialized)) {
        return;
    }

    for (uint8_t i = 0U; i < RELAY_ID_COUNT; i++) {
        handle->state[i] = RELAY_OFF;
        LL_GPIO_SetOutputPin(handle->pin_cfg[i].port,
                             handle->pin_cfg[i].pin);
    }
}
