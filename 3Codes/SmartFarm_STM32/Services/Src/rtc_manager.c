/**
 * @file    rtc_manager.c
 * @brief   RTC management service implementation.
 * @author  Group SmartFarm
 * @date    2026-07-01
 */

#include "rtc_manager.h"
#include "debug_log.h"
#include "app_config.h"
#include "rtc.h"

/* ============================================================================
 *   PRIVATE HELPERS
 * ============================================================================ */

static RTC_HandleTypeDef* get_hrtc(const RtcManager_t *mgr)
{
    return (RTC_HandleTypeDef*)mgr->hrtc;
}

/* ============================================================================
 *   PUBLIC API
 * ============================================================================ */

ErrorCode_t rtc_manager_init(RtcManager_t *mgr, void *hrtc)
{
    if ((mgr == NULL) || (hrtc == NULL))
    {
        return ERR_INVALID_PARAM;
    }

    mgr->hrtc = hrtc;
    mgr->initialized = true;

    /* Verify RTC is working */
    RTC_TimeTypeDef dummy_time;
    if (HAL_RTC_GetTime(get_hrtc(mgr), &dummy_time, RTC_FORMAT_BIN) != HAL_OK)
    {
        debug_log("rtc_manager: RTC not responding\r\n");
        return ERR_HW_INIT_FAIL;
    }

    debug_log("rtc_manager: init OK\r\n");
    return ERR_NONE;
}

ErrorCode_t rtc_manager_get_time(const RtcManager_t *mgr,
                                  TimeOfDay_t        *time_out)
{
    if ((mgr == NULL) || (time_out == NULL) || (!mgr->initialized))
    {
        return ERR_INVALID_PARAM;
    }

    RTC_TimeTypeDef rtc_time;
    if (HAL_RTC_GetTime(get_hrtc(mgr), &rtc_time, RTC_FORMAT_BIN) != HAL_OK)
    {
        return ERR_HW_INIT_FAIL;
    }

    time_out->hour   = rtc_time.Hours;
    time_out->minute = rtc_time.Minutes;
    time_out->second = rtc_time.Seconds;

    return ERR_NONE;
}

ErrorCode_t rtc_manager_get_date(const RtcManager_t *mgr,
                                  DateOfDay_t        *date_out)
{
    if ((mgr == NULL) || (date_out == NULL) || (!mgr->initialized))
    {
        return ERR_INVALID_PARAM;
    }

    RTC_DateTypeDef rtc_date;
    if (HAL_RTC_GetDate(get_hrtc(mgr), &rtc_date, RTC_FORMAT_BIN) != HAL_OK)
    {
        return ERR_HW_INIT_FAIL;
    }

    date_out->year    = rtc_date.Year;
    date_out->month   = rtc_date.Month;
    date_out->day     = rtc_date.Date;
    date_out->weekday = rtc_date.WeekDay;   /* HAL: 1=Mon..7=Sun */

    return ERR_NONE;
}

ErrorCode_t rtc_manager_set_time(RtcManager_t      *mgr,
                                  const TimeOfDay_t *time)
{
    if ((mgr == NULL) || (time == NULL) || (!mgr->initialized))
    {
        return ERR_INVALID_PARAM;
    }

    RTC_TimeTypeDef rtc_time;
    rtc_time.Hours   = time->hour;
    rtc_time.Minutes = time->minute;
    rtc_time.Seconds = time->second;
    rtc_time.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    rtc_time.StoreOperation = RTC_STOREOPERATION_RESET;

    if (HAL_RTC_SetTime(get_hrtc(mgr), &rtc_time, RTC_FORMAT_BIN) != HAL_OK)
    {
        return ERR_HW_INIT_FAIL;
    }

    return ERR_NONE;
}

ErrorCode_t rtc_manager_set_date(RtcManager_t      *mgr,
                                  const DateOfDay_t *date)
{
    if ((mgr == NULL) || (date == NULL) || (!mgr->initialized))
    {
        return ERR_INVALID_PARAM;
    }

    RTC_DateTypeDef rtc_date;
    rtc_date.Year    = date->year;
    rtc_date.Month   = date->month;
    rtc_date.Date    = date->day;
    rtc_date.WeekDay = date->weekday;

    if (HAL_RTC_SetDate(get_hrtc(mgr), &rtc_date, RTC_FORMAT_BIN) != HAL_OK)
    {
        return ERR_HW_INIT_FAIL;
    }

    return ERR_NONE;
}

bool rtc_manager_is_forbidden(const RtcManager_t *mgr)
{
    if ((mgr == NULL) || (!mgr->initialized))
    {
        return false;
    }

    TimeOfDay_t now;
    if (rtc_manager_get_time(mgr, &now) != ERR_NONE)
    {
        return false;
    }

    /* Forbidden period: [FORBIDDEN_START_HOUR, FORBIDDEN_END_HOUR) */
    if (FORBIDDEN_START_HOUR < FORBIDDEN_END_HOUR)
    {
        return ((now.hour >= FORBIDDEN_START_HOUR) &&
                (now.hour < FORBIDDEN_END_HOUR));
    }
    else
    {
        /* wrap-around case (not typical) */
        return ((now.hour >= FORBIDDEN_START_HOUR) ||
                (now.hour < FORBIDDEN_END_HOUR));
    }
    return false; 
}

uint32_t rtc_manager_time_to_seconds(const TimeOfDay_t *time)
{
    if (time == NULL)
    {
        return 0U;
    }
    return ((uint32_t)time->hour * 3600U) +
           ((uint32_t)time->minute * 60U) +
           (uint32_t)time->second;
}

int8_t rtc_manager_compare_time(const TimeOfDay_t *a, const TimeOfDay_t *b)
{
    if ((a == NULL) || (b == NULL))
    {
        return 0;
    }

    uint32_t sec_a = rtc_manager_time_to_seconds(a);
    uint32_t sec_b = rtc_manager_time_to_seconds(b);

    if (sec_a < sec_b) return -1;
    if (sec_a > sec_b) return 1;
    return 0;
}
