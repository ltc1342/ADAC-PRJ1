/**
 * @file    display_manager.h
 * @brief   OLED display management service (SH1106 128×64).
 * @author  ltc1342
 * @date    2026-07-01
 *
 * @note    The display manager is device-agnostic: it receives function
 *          pointers for the SH1106 draw primitives rather than calling the
 *          driver directly.  This mirrors the sensor_manager pattern and
 *          keeps the app layer independent of the device layer.
 *
 *          Screen layout (default):
 *          ┌──────────────────────────┐
 *          │ Line 0: HH:MM:SS  MODE  │  ← RTC time + mode icon
 *          │ Line 1: T:25.3°C H:58%  │  ← Temperature & humidity
 *          │ Line 2: L:320lux        │  ← Illuminance
 *          │ Line 3: Soil: 45%       │  ← Soil moisture
 *          │ Line 4: Pump:ON Mist:OFF│  ← Relay status
 *          │ Line 5: <error banner>  │  ← Shown only on sensor error
 *          └──────────────────────────┘
 *
 *          Usage:
 *          @code
 *            static DisplayManager_t disp_mgr;
 *
 *            DisplayDriverFns_t fns = {
 *                .clear      = sh1106_clear,
 *                .draw_str   = sh1106_draw_string,
 *                .draw_line  = sh1106_draw_hline,
 *                .refresh    = sh1106_refresh,
 *                .handle     = &g_sh1106,
 *            };
 *            display_manager_init(&disp_mgr, &fns, 500U);
 *            display_manager_show_splash(&disp_mgr);
 *
 *            // In main loop:
 *            display_manager_update(&disp_mgr, &sensor_data,
 *                                   &relay_status, mode, &rtc_time);
 *          @endcode
 */

#ifndef DISPLAY_MANAGER_H_
#define DISPLAY_MANAGER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "app_types.h"
#include "rtc_manager.h"   /* TimeOfDay_t  */

/* ============================================================================
 *   DISPLAY DRIVER FUNCTION-POINTER TYPEDEFS
 * ============================================================================ */

/**
 * @brief  Clear the frame buffer (does not refresh the panel).
 * @param  handle  Opaque driver handle.
 */
typedef void (*disp_clear_fn_t)(void *handle);

/**
 * @brief  Draw a null-terminated ASCII string at pixel position (x, y).
 * @param  handle  Opaque driver handle.
 * @param  x       Column in pixels.
 * @param  y       Row in pixels.
 * @param  str     Null-terminated ASCII string.
 */
typedef void (*disp_draw_str_fn_t)(void *handle,
                                    uint8_t     x,
                                    uint8_t     y,
                                    const char *str);

/**
 * @brief  Draw a horizontal line at row y spanning from x0 to x1.
 * @param  handle  Opaque driver handle.
 * @param  y       Row in pixels.
 * @param  x0      Start column.
 * @param  x1      End column.
 */
typedef void (*disp_draw_hline_fn_t)(void   *handle,
                                      uint8_t y,
                                      uint8_t x0,
                                      uint8_t x1);

/**
 * @brief  Push the frame buffer to the OLED panel.
 * @param  handle  Opaque driver handle.
 */
typedef void (*disp_refresh_fn_t)(void *handle);

/* ============================================================================
 *   DRIVER FUNCTION TABLE
 * ============================================================================ */

/**
 * @brief  Set of SH1106 operations required by the display manager.
 */
typedef struct {
    void               *handle;       /**< Opaque Sh1106Handle_t* (may be NULL
                                           if driver uses global state)       */
    disp_clear_fn_t     clear;        /**< Required                          */
    disp_draw_str_fn_t  draw_str;     /**< Required                          */
    disp_draw_hline_fn_t draw_hline;  /**< Optional (pass NULL to skip)      */
    disp_refresh_fn_t   refresh;      /**< Required                          */
} DisplayDriverFns_t;

/* ============================================================================
 *   MANAGER INSTANCE
 * ============================================================================ */

/**
 * @brief  Display manager runtime state.
 */
typedef struct {
    DisplayDriverFns_t driver;              /**< Injected draw functions       */
    uint32_t           refresh_interval_ms; /**< Min ms between refreshes      */
    uint32_t           last_refresh_ms;     /**< timing_get_ms() at last call  */
    bool               initialized;
} DisplayManager_t;

/* ============================================================================
 *   API
 * ============================================================================ */

/**
 * @brief  Initialise display manager.
 * @param  mgr                 Manager instance (must not be NULL).
 * @param  driver              Draw function table (clear/draw_str/refresh
 *                             must not be NULL).
 * @param  refresh_interval_ms Minimum period between screen refreshes [ms].
 *                             Pass 0 to refresh every call.
 * @return ERR_NONE on success, ERR_INVALID_PARAM on NULL mandatory function.
 */
ErrorCode_t display_manager_init(DisplayManager_t         *mgr,
                                  const DisplayDriverFns_t *driver,
                                  uint32_t                  refresh_interval_ms);

/**
 * @brief  Render a full sensor + relay status screen.
 * @param  mgr     Initialised manager.
 * @param  data    Latest sensor snapshot (may be NULL → shows dashes).
 * @param  relay   Latest relay status   (may be NULL → shows dashes).
 * @param  mode    Current operating mode.
 * @param  time    Current RTC time      (may be NULL → omitted).
 * @note   Rate-limited by refresh_interval_ms; safe to call every loop tick.
 */
void display_manager_update(DisplayManager_t   *mgr,
                             const SensorData_t *data,
                             const RelayStatus_t *relay,
                             ControlMode_t        mode,
                             const TimeOfDay_t   *time);

/**
 * @brief  Show the startup splash screen (project name + version).
 * @param  mgr  Initialised manager.
 */
void display_manager_show_splash(DisplayManager_t *mgr);

/**
 * @brief  Show a full-screen error message.
 * @param  mgr  Initialised manager.
 * @param  msg  Null-terminated ASCII error text (≤ 3 lines of 21 chars).
 */
void display_manager_show_error(DisplayManager_t *mgr, const char *msg);

/**
 * @brief  Clear the screen immediately (no rate limiting).
 * @param  mgr  Initialised manager.
 */
void display_manager_clear(DisplayManager_t *mgr);

/**
 * @brief  Force the next update() call to refresh regardless of interval.
 * @param  mgr  Initialised manager.
 */
void display_manager_force_refresh(DisplayManager_t *mgr);

#ifdef __cplusplus
}
#endif

#endif /* DISPLAY_MANAGER_H_ */
