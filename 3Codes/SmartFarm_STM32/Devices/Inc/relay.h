/**
 * @file    relay.h
 * @brief   Relay driver (Active-LOW, 5V, LL GPIO).
 * @author  ltc1342
 * @date    2026-07-01
 *
 * @note    Active-LOW wiring: GPIO LOW → relay coil energised (ON).
 *          GPIO HIGH → relay coil off (OFF) – safe/default state.
 *
 *          Pin mapping (project default):
 *            RELAY_ID_PUMP  → PB0
 *            RELAY_ID_MIST  → PB1
 *
 *          GPIO must be configured as Push-Pull Output before calling relay_init().
 *          (CubeMX / BSP layer handles GPIO init.)
 */

#ifndef RELAY_H_
#define RELAY_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_ll_gpio.h"
#include "app_types.h"

/* ============================================================================
 *   RELAY INDEX CONSTANTS
 * ============================================================================ */

#define RELAY_ID_PUMP   0U   /**< Water pump relay index  */
#define RELAY_ID_MIST   1U   /**< Mist maker relay index  */
#define RELAY_ID_COUNT  2U   /**< Total number of relays  */

/* ============================================================================
 *   TYPES
 * ============================================================================ */

/**
 * @brief  Hardware pin descriptor for one relay.
 */
typedef struct {
    GPIO_TypeDef *port;   /**< GPIO port (e.g. GPIOB)            */
    uint32_t      pin;    /**< LL pin mask (e.g. LL_GPIO_PIN_0)  */
} RelayPin_t;

/**
 * @brief  Relay driver instance.
 * @note   Caller allocates storage (static or on stack).
 */
typedef struct {
    RelayPin_t   pin_cfg[RELAY_ID_COUNT];   /**< GPIO descriptors          */
    RelayState_t state[RELAY_ID_COUNT];     /**< Logical states (OFF/ON)   */
    bool         initialized;               /**< Guard against unInit use  */
} RelayHandle_t;

/* ============================================================================
 *   API
 * ============================================================================ */

/**
 * @brief  Initialise relay driver and force all relays to OFF.
 * @param  handle  Caller-supplied handle storage (must not be NULL).
 * @param  pump    GPIO config for the pump relay.
 * @param  mist    GPIO config for the mist relay.
 * @return ERR_NONE on success, ERR_INVALID_PARAM if handle is NULL.
 */
ErrorCode_t relay_init(RelayHandle_t *handle,
                       RelayPin_t     pump,
                       RelayPin_t     mist);

/**
 * @brief  Set a relay to the requested logical state.
 * @param  handle    Initialised relay handle.
 * @param  relay_id  RELAY_ID_PUMP or RELAY_ID_MIST.
 * @param  state     RELAY_ON or RELAY_OFF.
 * @return ERR_NONE on success, ERR_INVALID_PARAM on bad args.
 */
ErrorCode_t relay_set(RelayHandle_t *handle,
                      uint8_t        relay_id,
                      RelayState_t   state);

/**
 * @brief  Read back the current logical state of a relay.
 * @param  handle     Initialised relay handle.
 * @param  relay_id   RELAY_ID_PUMP or RELAY_ID_MIST.
 * @param  state_out  Output (must not be NULL).
 * @return ERR_NONE on success.
 */
ErrorCode_t relay_get_state(const RelayHandle_t *handle,
                             uint8_t              relay_id,
                             RelayState_t        *state_out);

/**
 * @brief  Toggle a relay (ON→OFF or OFF→ON).
 * @param  handle    Initialised relay handle.
 * @param  relay_id  RELAY_ID_PUMP or RELAY_ID_MIST.
 * @return ERR_NONE on success.
 */
ErrorCode_t relay_toggle(RelayHandle_t *handle, uint8_t relay_id);

/**
 * @brief  Force ALL relays to OFF (safe-state call, e.g. on fault).
 * @param  handle  Initialised relay handle (silently ignored if NULL).
 */
void relay_all_off(RelayHandle_t *handle);

#ifdef __cplusplus
}
#endif

#endif /* RELAY_H_ */
