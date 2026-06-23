/**
 * @file    display_manager.c
 * @brief   OLED display management using SH1106 driver.
 * @author  Group SmartFarm
 * @date    2026-07-01
 */

#include "display_manager.h"
#include "timing.h"
#include "debug_log.h"
#include "sh1106.h"
#include "fonts.h"
#include <stdio.h>
#include <string.h>

/* ============================================================================
 *   PRIVATE DATA
 * ============================================================================ */

/** Font used for all text (7x10 is compact) */
static FontDef_t *g_font = &font_7x10;

/** Color for text (white on black) */
static const Sh1106Color_t g_text_color = SH1106_COLOR_WHITE;

/* ============================================================================
 *   PRIVATE HELPERS
 * ============================================================================ */

/**
 * @brief  Draw a string at pixel (x, y) using default font and color.
 */
static void draw_string(uint8_t x, uint8_t y, const char *str)
{
    sh1106_goto_xy(x, y);
    (void)sh1106_puts(str, g_font, g_text_color);
}

/**
 * @brief  Clear the entire frame buffer.
 */
static void clear_screen(void)
{
    sh1106_fill(SH1106_COLOR_BLACK);
}

/**
 * @brief  Refresh the display (copy frame buffer to hardware).
 */
static void refresh_screen(void)
{
    (void)sh1106_update_screen();
}

/* ============================================================================
 *   PUBLIC API
 * ============================================================================ */

ErrorCode_t display_manager_init(DisplayManager_t         *mgr,
                                  const DisplayDriverFns_t *driver,
                                  uint32_t                  refresh_interval_ms)
{
    /* Driver parameter is ignored – we use SH1106 directly.
     * Keep for API compatibility, but mark as unused. */
    (void)driver;

    if (mgr == NULL)
    {
        return ERR_INVALID_PARAM;
    }

    mgr->refresh_interval_ms = refresh_interval_ms;
    mgr->last_refresh_ms = 0U;
    mgr->initialized = true;

    /* Initialise SH1106 (if not already done) – safe to call multiple times */
    (void)sh1106_init();

    /* Clear and refresh */
    clear_screen();
    refresh_screen();

    debug_log("display_manager: init OK (SH1106)\r\n");
    return ERR_NONE;
}

void display_manager_update(DisplayManager_t   *mgr,
                             const SensorData_t *data,
                             const RelayStatus_t *relay,
                             ControlMode_t        mode,
                             const TimeOfDay_t   *time)
{
    if ((mgr == NULL) || (!mgr->initialized))
    {
        return;
    }

    /* Rate limit */
    uint32_t now = timing_get_ms();
    if ((now - mgr->last_refresh_ms) < mgr->refresh_interval_ms)
    {
        return;
    }
    mgr->last_refresh_ms = now;

    /* Clear frame buffer */
    clear_screen();

    /* Line 0: Time & Mode */
    char line0[32];

    if (time != NULL)
    {
        snprintf(line0, sizeof(line0), "%02u:%02u:%02u  ",
                 time->hour, time->minute, time->second);
    }
    else
    {
        strcpy(line0, "        ");
    }
    const char *mode_str;
    switch (mode)
    {
        case MODE_AUTO:     mode_str = "AUTO"; break;
        case MODE_MANUAL:   mode_str = "MAN";  break;
        case MODE_SCHEDULE: mode_str = "SCH";  break;
        default:            mode_str = "???";  break;
    }
    strcat(line0, mode_str);
    draw_string(0, 0, line0);

    /* Line 1: Temperature & Humidity */
    char line1[32];
    if (data != NULL)
    {
        snprintf(line1, sizeof(line1), "T:%.1fC H:%.1f%%",
                 data->temperature, data->humidity);
    }
    else
    {
        strcpy(line1, "T:--.-C H:--.-%");
    }
    draw_string(0, 10, line1);

    /* Line 2: Light */
    char line2[32];
    if (data != NULL)
    {
        snprintf(line2, sizeof(line2), "L:%.0flx", data->light_lux);
    }
    else
    {
        strcpy(line2, "L:-----lx");
    }
    draw_string(0, 20, line2);

    /* Line 3: Soil moisture */
    char line3[32];
    if (data != NULL)
    {
        snprintf(line3, sizeof(line3), "Soil:%.0f%%", data->soil_moisture);
    }
    else
    {
        strcpy(line3, "Soil:---%");
    }
    draw_string(0, 30, line3);

    /* Line 4: Relay status */
    char line4[32];
    if (relay != NULL)
    {
        snprintf(line4, sizeof(line4), "Pump:%s Mist:%s",
                 (relay->pump == RELAY_ON) ? "ON " : "OFF",
                 (relay->mist == RELAY_ON) ? "ON " : "OFF");
    }
    else
    {
        strcpy(line4, "Pump:-- Mist:--");
    }
    draw_string(0, 40, line4);

    /* Line 5: Error banner */
    if (data != NULL)
    {
        if ((data->dht11_error != SENSOR_OK) ||
            (data->bh1750_error != SENSOR_OK) ||
            (data->adc_error != SENSOR_OK))
        {
            draw_string(0, 54, "SENSOR ERR!");
        }
        else
        {
            draw_string(0, 54, "All OK");
        }
    }

    /* Refresh screen */
    refresh_screen();
}

void display_manager_show_splash(DisplayManager_t *mgr)
{
    if ((mgr == NULL) || (!mgr->initialized))
    {
        return;
    }

    clear_screen();
    draw_string(20, 20, "SmartFarm");
    draw_string(25, 35, "v1.0");
    refresh_screen();
}

void display_manager_show_error(DisplayManager_t *mgr, const char *msg)
{
    if ((mgr == NULL) || (!mgr->initialized) || (msg == NULL))
    {
        return;
    }

    clear_screen();
    draw_string(0, 20, "ERROR");
    draw_string(0, 35, msg);
    refresh_screen();
}

void display_manager_clear(DisplayManager_t *mgr)
{
    if ((mgr == NULL) || (!mgr->initialized))
    {
        return;
    }
    clear_screen();
    refresh_screen();
}

void display_manager_force_refresh(DisplayManager_t *mgr)
{
    if ((mgr == NULL) || (!mgr->initialized))
    {
        return;
    }
    mgr->last_refresh_ms = 0U;
}
