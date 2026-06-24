/**
 * @file power_manager.c
 * @brief Deep-sleep power management implementation.
 *
 * Deep sleep wakeup sources configured:
 *   Primary   : Timer wakeup (SLEEP_TIMER_US)
 *   (Optional): EXT0 on GPIO0 (e.g. BOOT button) — uncomment to enable
 *
 * RTC domain notes:
 *   - RTC SLOW memory (8 KB) and RTC FAST memory (8 KB) survive deep sleep.
 *   - Mark variables with RTC_DATA_ATTR to retain them across wakes.
 *   - Example: static RTC_DATA_ATTR uint32_t s_boot_count = 0;
 *
 * Power sequence before deep sleep:
 *   1. esp_wifi_stop()        → shut down RF (saves ~160 mA)
 *   2. uart_wait_tx_done()    → flush TX FIFO so last byte reaches STM32
 *   3. esp_sleep_enable_*()   → arm wakeup source
 *   4. esp_deep_sleep_start() → powers down (returns only via reset)
 */

#include "power_manager.h"
#include "app_config.h"

#include "esp_sleep.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "driver/uart.h"

static const char *TAG = "POWER_MGR";

/* Optional: count wakes across deep-sleep cycles (persists in RTC RAM) */
static RTC_DATA_ATTR uint32_t s_wake_count = 0;

/* ── Public API ──────────────────────────────────────────────────────────── */

bool power_manager_is_wakeup(void)
{
    return (esp_reset_reason() == ESP_RST_DEEPSLEEP);
}

void power_manager_log_wakeup(void)
{
    s_wake_count++;
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

    switch (cause) {
    case ESP_SLEEP_WAKEUP_TIMER:
        ESP_LOGI(TAG, "Wakeup #%lu — timer (%.1f s period)",
                 (unsigned long)s_wake_count,
                 (double)SLEEP_TIMER_US / 1e6);
        break;
    case ESP_SLEEP_WAKEUP_EXT0:
        ESP_LOGI(TAG, "Wakeup #%lu — EXT0 GPIO",
                 (unsigned long)s_wake_count);
        break;
    default:
        ESP_LOGI(TAG, "Wakeup #%lu — cause=%d",
                 (unsigned long)s_wake_count, (int)cause);
        break;
    }
}

void power_manager_deep_sleep(void)
{
    ESP_LOGI(TAG, "Preparing for deep sleep (%.1f s)...",
             (double)SLEEP_TIMER_US / 1e6);

    /* Step 1: Flush UART TX so STM32 receives the last byte */
    uart_wait_tx_done(STM32_UART_PORT, pdMS_TO_TICKS(50));

    /* Step 2: Stop Wi-Fi RF to avoid ~160 mA current spike during sleep */
    esp_wifi_stop();

    /* Step 3: Arm timer wakeup */
    esp_sleep_enable_timer_wakeup(SLEEP_TIMER_US);

    /*
     * Optional EXT0 wakeup (e.g. STM32 pulls GPIO0 low to request early wake):
     * esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);
     */

    ESP_LOGI(TAG, "Entering deep sleep now. Next wake in %llu ms.",
             SLEEP_TIMER_US / 1000ULL);

    /* Step 4: Enter deep sleep — this call does NOT return */
    esp_deep_sleep_start();

    /* Unreachable — added to satisfy static analysis tools */
    __builtin_unreachable();
}
