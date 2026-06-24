/**
 * @file    schedule_manager.c
 * @brief   Weekly schedule manager implementation.
 * @author  Group SmartFarm
 * @date    2026-07-01
 */

#include "schedule_manager.h"
#include "debug_log.h"
#include <string.h>

/* ============================================================================
 *   PRIVATE HELPERS
 * ============================================================================ */

static bool is_weekday_match(uint8_t weekday, uint8_t day_mask)
{
    /* weekday: 1=Mon..7=Sun, day_mask: bit0=Mon..bit6=Sun */
    if ((weekday < 1) || (weekday > 7))
    {
        return false;
    }
    uint8_t bit = (uint8_t)(1U << (weekday - 1U));
    return ((day_mask & bit) != 0U);
}

/* ============================================================================
 *   PUBLIC API
 * ============================================================================ */

ErrorCode_t schedule_manager_init(ScheduleManager_t    *mgr,
                                   const ScheduleEntry_t *entries,
                                   uint8_t               count)
{
    if ((mgr == NULL) || (entries == NULL) ||
        (count == 0U) || (count > SCHEDULE_MAX_ENTRIES))
    {
        return ERR_INVALID_PARAM;
    }

    mgr->entries = entries;
    mgr->count   = count;
    mgr->enabled = true;
    mgr->initialized = true;

    /* Clear state */
    for (uint8_t i = 0U; i < count; i++)
    {
        mgr->state[i].fired_this_minute = false;
        mgr->state[i].last_fired_hour   = 0U;
        mgr->state[i].last_fired_minute = 0U;
    }

    debug_log("schedule_manager: init with %u entries\r\n", count);
    return ERR_NONE;
}

bool schedule_manager_should_water(ScheduleManager_t *mgr,
                                   const TimeOfDay_t *now,
                                   const DateOfDay_t *today)
{
    if ((mgr == NULL) || (!mgr->initialized) || (!mgr->enabled) ||
        (now == NULL) || (today == NULL))
    {
        return false;
    }

    /* Loop through all entries */
    for (uint8_t i = 0U; i < mgr->count; i++)
    {
        const ScheduleEntry_t *entry = &mgr->entries[i];

        /* Check weekday mask */
        if (!is_weekday_match(today->weekday, entry->weekday_mask))
        {
            continue;
        }

        /* Check if current time is within the watering window */
        uint32_t start_sec = ((uint32_t)entry->hour * 3600U) +
                             ((uint32_t)entry->minute * 60U);
        uint32_t now_sec = rtc_manager_time_to_seconds(now);
        uint32_t end_sec = start_sec + entry->duration_s;

        bool in_window;
        if (end_sec <= 86400U)
        {
            in_window = (now_sec >= start_sec) && (now_sec < end_sec);
        }
        else
        {
            /* crosses midnight */
            in_window = (now_sec >= start_sec) || (now_sec < (end_sec - 86400U));
        }

        if (!in_window)
        {
            continue;
        }

        /* Check if already fired in this minute */
        if (mgr->state[i].fired_this_minute)
        {
            continue;
        }

        /* Fire! */
        mgr->state[i].fired_this_minute = true;
        mgr->state[i].last_fired_hour   = now->hour;
        mgr->state[i].last_fired_minute = now->minute;
        return true;
    }

    return false;
}

bool schedule_manager_get_next(const ScheduleManager_t *mgr,
                               const TimeOfDay_t        *after,
                               const DateOfDay_t        *today,
                               TimeOfDay_t              *next_out)
{
    if ((mgr == NULL) || (!mgr->initialized) || (after == NULL) ||
        (today == NULL) || (next_out == NULL) || (mgr->count == 0U))
    {
        return false;
    }

    uint32_t after_sec = rtc_manager_time_to_seconds(after);
    uint32_t best_sec = UINT32_MAX;
    bool found = false;

    for (uint8_t i = 0U; i < mgr->count; i++)
    {
        const ScheduleEntry_t *entry = &mgr->entries[i];
        if (!is_weekday_match(today->weekday, entry->weekday_mask))
        {
            continue;
        }

        uint32_t entry_sec = ((uint32_t)entry->hour * 3600U) +
                             ((uint32_t)entry->minute * 60U);
        if (entry_sec <= after_sec)
        {
            continue;
        }

        if (entry_sec < best_sec)
        {
            best_sec = entry_sec;
            next_out->hour = entry->hour;
            next_out->minute = entry->minute;
            next_out->second = 0U;
            found = true;
        }
    }

    return found;
}

void schedule_manager_set_enabled(ScheduleManager_t *mgr, bool enabled)
{
    if ((mgr == NULL) || (!mgr->initialized))
    {
        return;
    }
    mgr->enabled = enabled;
}

bool schedule_manager_is_enabled(const ScheduleManager_t *mgr)
{
    if ((mgr == NULL) || (!mgr->initialized))
    {
        return false;
    }
    return mgr->enabled;
}

void schedule_manager_reset_daily(ScheduleManager_t *mgr)
{
    if ((mgr == NULL) || (!mgr->initialized))
    {
        return;
    }

    for (uint8_t i = 0U; i < mgr->count; i++)
    {
        mgr->state[i].fired_this_minute = false;
    }
}
