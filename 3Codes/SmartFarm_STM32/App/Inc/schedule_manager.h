/**
 * @file    schedule_manager.h
 * @brief   Weekly watering schedule service.
 * @author  ltc1342
 * @date    2026-07-01
 *
 * @note    Schedule entries are defined in app_config.h (ScheduleEntry_t).
 *          The manager matches current RTC time against the table and
 *          signals whether a watering event should start right now.
 *
 *          A watering event fires once per scheduled time; a "fired" flag
 *          prevents re-triggering within the same minute.
 *
 *          The forbidden watering period (10:00–15:00) is enforced by
 *          rtc_manager_is_forbidden() at the control layer, not here.
 *
 *          Weekday mask (app_config.h / ScheduleEntry_t):
 *            Bit 0 = Monday … Bit 6 = Sunday  (1 = enabled)
 *            0x7F = every day, 0x1F = Mon–Fri, 0x60 = Sat–Sun
 *
 *          Usage:
 *          @code
 *            static ScheduleManager_t sched_mgr;
 *            schedule_manager_init(&sched_mgr,
 *                                  default_schedule,
 *                                  SCHEDULE_ENTRIES);
 *
 *            // In main loop (call at least once per minute):
 *            TimeOfDay_t now;  DateOfDay_t today;
 *            rtc_manager_get_time(...);  rtc_manager_get_date(...);
 *
 *            if (schedule_manager_should_water(&sched_mgr, &now, &today)) {
 *                relay_manager_request(&relay_mgr, RELAY_ID_PUMP, RELAY_ON);
 *            }
 *          @endcode
 */

#ifndef SCHEDULE_MANAGER_H_
#define SCHEDULE_MANAGER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "app_types.h"    /* TimeOfDay_t              */
#include "app_config.h"   /* ScheduleEntry_t, SCHEDULE_ENTRIES */
#include "rtc_manager.h"  /* DateOfDay_t              */

/* ============================================================================
 *   CONSTANTS
 * ============================================================================ */

/**
 * @brief  Maximum number of schedule entries the manager can hold.
 *         Must be ≥ SCHEDULE_ENTRIES defined in app_config.h.
 */
#define SCHEDULE_MAX_ENTRIES   8U

/* ============================================================================
 *   TYPES
 * ============================================================================ */

/**
 * @brief  Runtime tracking per schedule entry.
 */
typedef struct {
    bool     fired_this_minute;   /**< Prevent double-fire in same minute */
    uint8_t  last_fired_hour;     /**< Hour of last fire event            */
    uint8_t  last_fired_minute;   /**< Minute of last fire event          */
} ScheduleEntryState_t;

/**
 * @brief  Schedule manager instance.
 */
typedef struct {
    const ScheduleEntry_t *entries;                         /**< Table (app_config.h)         */
    uint8_t                count;                           /**< Active entry count           */
    ScheduleEntryState_t   state[SCHEDULE_MAX_ENTRIES];     /**< Per-entry runtime state      */
    bool                   enabled;                         /**< Master enable/disable switch */
    bool                   initialized;
} ScheduleManager_t;

/* ============================================================================
 *   API
 * ============================================================================ */

/**
 * @brief  Initialise schedule manager with a watering table.
 * @param  mgr      Manager instance (must not be NULL).
 * @param  entries  Pointer to schedule array (see app_config.h).
 * @param  count    Number of entries (≤ SCHEDULE_MAX_ENTRIES).
 * @return ERR_NONE on success, ERR_INVALID_PARAM on bad args.
 */
ErrorCode_t schedule_manager_init(ScheduleManager_t    *mgr,
                                   const ScheduleEntry_t *entries,
                                   uint8_t               count);

/**
 * @brief  Evaluate whether the pump should activate right now.
 * @param  mgr    Initialised manager.
 * @param  now    Current time (from rtc_manager_get_time).
 * @param  today  Current date (from rtc_manager_get_date) – used for weekday.
 * @return true  – a schedule entry just triggered; activate pump.
 *         false – no trigger this cycle.
 * @note   Call at least once per minute.  Internally debounces per-minute.
 */
bool schedule_manager_should_water(ScheduleManager_t *mgr,
                                   const TimeOfDay_t *now,
                                   const DateOfDay_t *today);

/**
 * @brief  Find the next scheduled watering time after 'after'.
 * @param  mgr        Initialised manager.
 * @param  after      Reference time to search from.
 * @param  today      Current date (for weekday filtering).
 * @param  next_out   Destination for the next trigger time.
 * @return true  – a future entry was found; *next_out is valid.
 *         false – schedule is empty or all entries already passed today.
 */
bool schedule_manager_get_next(const ScheduleManager_t *mgr,
                               const TimeOfDay_t        *after,
                               const DateOfDay_t        *today,
                               TimeOfDay_t              *next_out);

/**
 * @brief  Enable or disable the entire schedule (e.g. via MQTT command).
 * @param  mgr      Initialised manager.
 * @param  enabled  true = schedule active, false = all events suppressed.
 */
void schedule_manager_set_enabled(ScheduleManager_t *mgr, bool enabled);

/**
 * @brief  Return whether the schedule is currently enabled.
 * @param  mgr  Initialised manager.
 * @return true if enabled.
 */
bool schedule_manager_is_enabled(const ScheduleManager_t *mgr);

/**
 * @brief  Clear all fired-this-minute flags (call at midnight rollover).
 * @param  mgr  Initialised manager.
 */
void schedule_manager_reset_daily(ScheduleManager_t *mgr);

#ifdef __cplusplus
}
#endif

#endif /* SCHEDULE_MANAGER_H_ */
