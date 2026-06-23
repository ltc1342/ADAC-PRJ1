/**
 * @file    app_defs.h
 * @brief   Constant macros used across the system.
 * @author  Group SmartFarm
 * @date    2026-07-01
 *
 * @note    All values are compile‑time constants.
 */

#ifndef APP_DEFS_H
#define APP_DEFS_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 *   DEBUG OUTPUT SELECTION
 * ============================================================================ */
/** @brief Uncomment exactly one of the following macros to select debug output. */
#define DEBUG_SWO_ENABLE   1U   /* Use ITM / SWO (recommended) */
/* #define DEBUG_UART_ENABLE  1U */   /* Use UART (alternative) */

/* ============================================================================
 *   I2C ADDRESSES
 * ============================================================================ */
#define BH1750_I2C_ADDR     0x23U
#define SH1106_I2C_ADDR     0x3CU

/* ============================================================================
 *   SAMPLING & TIMEOUTS (milliseconds)
 * ============================================================================ */
#define SENSOR_READ_INTERVAL_MS  5000U
#define DHT11_TIMEOUT_MS         100U
#define BH1750_CONV_TIME_MS      150U
#define ADC_SAMPLE_WINDOW_MS     10U

/* ============================================================================
 *   RELAY PROTECTION
 * ============================================================================ */
#define RELAY_HYSTERESIS_MS      5000U
#define RELAY_DEFAULT_PULSE_MS   2000U

/* ============================================================================
 *   SOIL MOISTURE THRESHOLDS (%)
 * ============================================================================ */
#define SOIL_DRY_THRESHOLD       40U
#define SOIL_WET_THRESHOLD       70U

/* ============================================================================
 *   AIR HUMIDITY THRESHOLDS (%)
 * ============================================================================ */
#define HUMIDITY_LOW_THRESHOLD   50U
#define HUMIDITY_HIGH_THRESHOLD  70U

/* ============================================================================
 *   HEAT PROTECTION
 * ============================================================================ */
#define HEAT_TEMP_THRESHOLD_C    35
#define HEAT_LIGHT_THRESHOLD_LUX 10000U
#define HEAT_MIST_PULSE_ON_MS    30000U
#define HEAT_MIST_PULSE_OFF_MS   300000U

/* ============================================================================
 *   BUFFER SIZES
 * ============================================================================ */
#define UART_TX_BUFFER_SIZE      128U
#define UART_RX_BUFFER_SIZE      64U
#define OLED_DISPLAY_BUFFER_SIZE 1024U

/* ============================================================================
 *   HEARTBEAT
 * ============================================================================ */
#define HEARTBEAT_INTERVAL_MS    30000U

/* ============================================================================
 *   UART CONFIGURATION
 * ============================================================================ */
#define UART_BAUDRATE            115200U
#define SYSTEM_CLOCK_HZ          100000000U

#ifdef __cplusplus
}
#endif

#endif /* APP_DEFS_H */
