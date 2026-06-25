/**
 * @file    rtc_manager.c
 * @brief   RTC management service implementation.
 *
 * @details Thin wrapper around the STM32 HAL RTC driver providing:
 *           - Binary-format time/date read and write helpers.
 *           - Forbidden watering window check.
 *           - Utility functions for time arithmetic.
 *
 * @author  ltc1342
 * @date    2026-07-01
 *
 * @warning STM32 HAL RTC shadow register protocol:
 *          HAL_RTC_GetTime() MUST always be followed by HAL_RTC_GetDate()
 *          in every call path.  Omitting GetDate() leaves the shadow
 *          registers locked and subsequent GetTime() calls return stale
 *          values.  This rule is enforced in rtc_manager_get_time() and
 *          rtc_manager_get_date() via a combined read helper.
 */

#include "rtc_manager.h"
#include "app_config.h"      /* FORBIDDEN_START_HOUR, FORBIDDEN_END_HOUR */
#include "debug_log.h"
#include "stm32f4xx_hal.h"   /* RTC_HandleTypeDef, HAL_RTC_* */

/* ============================================================================
 *   PRIVATE HELPERS
 * ============================================================================ */

/**
 * @brief  Read both time and date registers in the mandatory HAL order.
 *
 * @details The STM32 RTC peripheral uses shadow registers that are only
 *          refreshed (unlocked) after a GetDate() call following a GetTime()
 *          call.  Reading time alone is therefore insufficient and will
 *          stall the shadow update pipeline.  This helper always reads both.
 *
 * @param  hrtc      HAL RTC handle (caller guarantees non-NULL).
 * @param  time_out  Destination for time values (may be NULL to discard).
 * @param  date_out  Destination for date values (may be NULL to discard).
 * @return ERR_NONE on success, ERR_HW_INIT_FAIL if HAL reports an error.
 */
static ErrorCode_t read_rtc_shadow(RTC_HandleTypeDef *hrtc,
                                    RTC_TimeTypeDef   *time_out,
                                    RTC_DateTypeDef   *date_out)
{
    RTC_TimeTypeDef local_time = {0};
    RTC_DateTypeDef local_date = {0};

    /* Always read time first – this locks the shadow registers */
    if (HAL_RTC_GetTime(hrtc, &local_time, RTC_FORMAT_BIN) != HAL_OK)
    {
        debug_log("[RtcMgr] HAL_RTC_GetTime failed\r\n");
        return ERR_HW_INIT_FAIL;
    }

    /* Always read date second – this unlocks the shadow registers */
    if (HAL_RTC_GetDate(hrtc, &local_date, RTC_FORMAT_BIN) != HAL_OK)
    {
        debug_log("[RtcMgr] HAL_RTC_GetDate failed\r\n");
        return ERR_HW_INIT_FAIL;
    }

    if (time_out != NULL)
    {
        *time_out = local_time;
    }

    if (date_out != NULL)
    {
        *date_out = local_date;
    }

    return ERR_NONE;
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

    mgr->hrtc        = hrtc;
    mgr->initialized = true;

    debug_log("[RtcMgr] initialised\r\n");
    return ERR_NONE;
}

ErrorCode_t rtc_manager_get_time(const RtcManager_t *mgr,
                                  TimeOfDay_t        *time_out)
{
    RTC_TimeTypeDef hal_time = {0};
    ErrorCode_t     err;

    if ((mgr == NULL) || (!mgr->initialized) || (time_out == NULL))
    {
        return ERR_INVALID_PARAM;
    }


    /* Cast stored void* back to the concrete HAL handle type.
     * The void* avoids pulling stm32f4xx_hal_rtc.h into every consumer
     * of rtc_manager.h – see header note. */
    RTC_HandleTypeDef *hrtc = (RTC_HandleTypeDef *)mgr->hrtc;

    err = read_rtc_shadow(hrtc, &hal_time, NULL);
    if (err != ERR_NONE)
    {
        return err;
    }
    RTC_TimeTypeDef rtc_time;

    if (HAL_RTC_GetTime(get_hrtc(mgr), &rtc_time, RTC_FORMAT_BIN) != HAL_OK)
    {
        return err;
    }

    time_out->hour   = hal_time.Hours;
    time_out->minute = hal_time.Minutes;
    time_out->second = hal_time.Seconds;

    return ERR_NONE;
}

ErrorCode_t rtc_manager_get_date(const RtcManager_t *mgr,
                                  DateOfDay_t        *date_out)
{
    RTC_TimeTypeDef hal_time = {0};
    RTC_DateTypeDef hal_date = {0};
    ErrorCode_t     err;

    if ((mgr == NULL) || (!mgr->initialized) || (date_out == NULL))
    {
        return ERR_INVALID_PARAM;
    }

    RTC_HandleTypeDef *hrtc = (RTC_HandleTypeDef *)mgr->hrtc;

    /* Must read time first (shadow register protocol) */
    err = read_rtc_shadow(hrtc, &hal_time, &hal_date);
    if (err != ERR_NONE)
    {
        return err;
    }

    date_out->year    = hal_date.Year;    /* 0–99 offset from 2000 */
    date_out->month   = hal_date.Month;   /* 1–12 (HAL uses BCD constants but BIN reads as value) */
    date_out->day     = hal_date.Date;    /* 1–31 */
    date_out->weekday = hal_date.WeekDay; /* 1=Mon … 7=Sun (HAL RTC_WEEKDAY_*) */

    return ERR_NONE;
}

ErrorCode_t rtc_manager_set_time(RtcManager_t      *mgr,
                                  const TimeOfDay_t *time)
{
    RTC_TimeTypeDef hal_time = {0};

    if ((mgr == NULL) || (!mgr->initialized) || (time == NULL))
    {
        return ERR_INVALID_PARAM;
    }

    /* Basic range validation */
    if ((time->hour > 23U) || (time->minute > 59U) || (time->second > 59U))
    {
        debug_log("[RtcMgr] set_time: invalid value %02u:%02u:%02u\r\n",
                  (unsigned)time->hour,
                  (unsigned)time->minute,
                  (unsigned)time->second);
        return ERR_INVALID_PARAM;
    }

    hal_time.Hours          = time->hour;
    hal_time.Minutes        = time->minute;
    hal_time.Seconds        = time->second;
    hal_time.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    hal_time.StoreOperation = RTC_STOREOPERATION_RESET;

    RTC_HandleTypeDef *hrtc = (RTC_HandleTypeDef *)mgr->hrtc;

    if (HAL_RTC_SetTime(hrtc, &hal_time, RTC_FORMAT_BIN) != HAL_OK)
    {
        debug_log("[RtcMgr] HAL_RTC_SetTime failed\r\n");
        return ERR_HW_INIT_FAIL;
    }

    debug_log("[RtcMgr] time set to %02u:%02u:%02u\r\n",
              (unsigned)time->hour,
              (unsigned)time->minute,
              (unsigned)time->second);

    return ERR_NONE;
}

ErrorCode_t rtc_manager_set_date(RtcManager_t      *mgr,
                                  const DateOfDay_t *date)
{
    RTC_DateTypeDef hal_date = {0};

    if ((mgr == NULL) || (!mgr->initialized) || (date == NULL))
    {
        return ERR_INVALID_PARAM;
    }

    /* Basic range validation */
    if ((date->month  < 1U)  || (date->month  > 12U) ||
        (date->day    < 1U)  || (date->day    > 31U) ||
        (date->weekday < 1U) || (date->weekday > 7U)  ||
        (date->year   > 99U))
    {
        debug_log("[RtcMgr] set_date: invalid value\r\n");
        return ERR_INVALID_PARAM;
    }

    hal_date.Year    = date->year;
    hal_date.Month   = date->month;
    hal_date.Date    = date->day;
    hal_date.WeekDay = date->weekday;

    RTC_HandleTypeDef *hrtc = (RTC_HandleTypeDef *)mgr->hrtc;

    if (HAL_RTC_SetDate(hrtc, &hal_date, RTC_FORMAT_BIN) != HAL_OK)
    {
        debug_log("[RtcMgr] HAL_RTC_SetDate failed\r\n");
        return ERR_HW_INIT_FAIL;
    }

    debug_log("[RtcMgr] date set to 20%02u-%02u-%02u (wd=%u)\r\n",
              (unsigned)date->year,
              (unsigned)date->month,
              (unsigned)date->day,
              (unsigned)date->weekday);

    return ERR_NONE;
}

bool rtc_manager_is_forbidden(const RtcManager_t *mgr)
{
    TimeOfDay_t now = {0};

    if ((mgr == NULL) || (!mgr->initialized))
    {
        /* Cannot determine time – default to NOT forbidden so we don't
         * block watering unnecessarily on a failed RTC read */
        return false;
    }

    if (rtc_manager_get_time(mgr, &now) != ERR_NONE)
    {
        debug_log("[RtcMgr] is_forbidden: failed to read time\r\n");
        return false;
    }

    /* Forbidden window: [FORBIDDEN_START_HOUR, FORBIDDEN_END_HOUR)
     * e.g. 10:00 – 15:00 means hours 10, 11, 12, 13, 14 are forbidden */
    return ((now.hour >= FORBIDDEN_START_HOUR) &&
            (now.hour <  FORBIDDEN_END_HOUR));
}

uint32_t rtc_manager_time_to_seconds(const TimeOfDay_t *time)
{
    if (time == NULL)
    {
        return 0U;
    }

    return ((uint32_t)time->hour   * 3600U) +
           ((uint32_t)time->minute *   60U) +
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

    if (sec_a < sec_b)
    {
        return -1;
    }

    if (sec_a > sec_b)
    {
        return 1;
    }

    return 0;
}
