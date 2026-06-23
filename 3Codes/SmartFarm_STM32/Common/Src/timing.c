/**
 * @file    bsp_dwt_delay.c
 * @brief   Implementation of DWT and SysTick delay functions.
 * @author  Group SmartFarm
 * @date    2026-07-01
 */

#include <timing.h>
#include "stm32f4xx_hal.h"
#include "core_cm4.h"

/* ============================================================================
 *   CONFIGURATION MACROS
 * ============================================================================ */

/* ============================================================================
 *   PRIVATE VARIABLES
 * ============================================================================ */

/** @brief CPU cycles per millisecond (updated at runtime). */
static uint32_t ms_coeff = 0U;

/** @brief CPU cycles per microsecond (updated at runtime). */
static uint32_t us_coeff = 0U;

/* ============================================================================
 *   DWT IMPLEMENTATION
 * ============================================================================ */

void timing_init(void) {
    /* Update SystemCoreClock and compute coefficients */
    SystemCoreClockUpdate();
    ms_coeff = SystemCoreClock / 1000U;
    us_coeff = SystemCoreClock / 1000000U;

    /* Enable trace and cycle counter */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static uint32_t timing_get_tick(void) {
    return DWT->CYCCNT;
}

uint32_t timing_get_us(void) {
    if (us_coeff == 0U) {
        return 0U;
    }
    return timing_get_tick() / us_coeff;
}

uint32_t timing_get_ms(void) {
    if (ms_coeff == 0U) {
        return 0U;
    }
    return timing_get_tick() / ms_coeff;
}

void timing_delay_us(uint32_t us) {
    if (us_coeff == 0U) {
        return;
    }
    uint32_t taget = us * us_coeff;
    uint32_t start = timing_get_tick();
    while ((timing_get_tick() - start) < taget) {
        /* Busy-wait */
    }
}

void timing_delay_ms(uint32_t ms) {
    if (ms_coeff == 0U) {
        return;
    }
    uint32_t taget = ms * ms_coeff;
    uint32_t start = timing_get_tick();
    while ((timing_get_tick() - start) < taget) {
        /* Busy-wait */
    }
}
