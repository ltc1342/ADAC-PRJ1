/**
 * @file    timing.h
 * @brief   DWT and SysTick delay functions.
 * @author  ltc1342
 * @date    2026-07-01
 *
 * @note    DWT is recommended for high‑resolution.
 *          SysTick can be used for millisecond counting with interrupts.
 */

#ifndef TIMING_H_
#define TIMING_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 *   TYPES
 * ============================================================================ */

/** @brief Non‑blocking delay descriptor. */
typedef struct {
    uint32_t timestamp;   /**< Start tick value */
    uint32_t delay;       /**< Delay duration in ticks */
} BspTime_t;

/* ============================================================================
 *   DWT FUNCTIONS (recommended)
 * ============================================================================ */

/**
 * @brief  Initialise DWT cycle counter.
 * @note   Must be called before using any DWT function.
 */
void timing_init(void);

/**
 * @brief  Get current DWT cycle count.
 * @return CPU cycle count.
 */
//uint32_t timing_get_tick(void);

/**
 * @brief  Get current time in microseconds (using DWT).
 * @return Microseconds.
 */
uint32_t timing_get_us(void);

/**
 * @brief  Get current time in milliseconds (using DWT).
 * @return Milliseconds.
 */
uint32_t timing_get_ms(void);

/**
 * @brief  Blocking delay in microseconds using DWT.
 * @param  delay  Delay in µs.
 */
void timing_delay_us(uint32_t us);

/**
 * @brief  Blocking delay in milliseconds using DWT.
 * @param  delay  Delay in ms.
 */
void timing_delay_ms(uint32_t ms);


#ifdef __cplusplus
}
#endif

#endif /* TIMING_H_ */
