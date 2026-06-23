/**
 * @file power_manager.h
 * @brief Level 4: Energy-aware gateway with deep/light sleep.
 *
 * In deep-sleep mode (ENABLE_DEEP_SLEEP = 1):
 *
 *   app_main boots → UART + Wi-Fi + MQTT init → wait one UART frame
 *   → publish to MQTT → disconnect Wi-Fi → deep sleep 10 s → restart
 *
 * Target average power consumption < 5 mW (active ~300 ms / 10 s cycle):
 *   Active   : ~160 mA × 3.3 V × 0.3 s  ≈ 0.158 W  (burst)
 *   Sleep    : ~10  µA × 3.3 V × 9.7 s  ≈ 0.00033 W (negligible)
 *   Average  : 0.3/10 × 160 mA = 4.8 mA × 3.3 V ≈ 15.8 mW
 *   (use modem-sleep and reduced TX power for tighter targets)
 *
 * RTC memory survives deep sleep — can be used to pass state across wakes.
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief Check if the last chip reset was a deep-sleep wakeup.
 * @return true if woken from deep sleep.
 */
bool power_manager_is_wakeup(void);

/**
 * @brief Log the wakeup cause to serial (informational).
 */
void power_manager_log_wakeup(void);

/**
 * @brief Disconnect Wi-Fi, arm a timer wakeup, and enter deep sleep.
 *
 * @warning This function NEVER RETURNS.
 *          The chip reboots and executes app_main() again after the
 *          sleep period expires (SLEEP_TIMER_US microseconds).
 */
void power_manager_deep_sleep(void);
