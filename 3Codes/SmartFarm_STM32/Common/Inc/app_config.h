/**
 * @file    app_config.h
 * @brief   Tunable parameters (PID, schedule, mode, etc.).
 * @author  Group SmartFarm
 * @date    2026-07-01
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "app_types.h"

/* ============================================================================
 *   PID GAINS
 * ============================================================================ */
#define PID_KP_DEFAULT  2.5f
#define PID_KI_DEFAULT  0.1f
#define PID_KD_DEFAULT  0.05f
#define PID_SAMPLE_TIME_MS 2000U

/* ============================================================================
 *   RTC SCHEDULE (Weekly Watering)
 * ============================================================================ */
typedef struct {
    uint8_t hour;
    uint8_t minute;
    uint8_t duration_s;
    uint8_t weekday_mask;
} ScheduleEntry_t;

#define SCHEDULE_ENTRIES 4U

/** @brief Default schedule (Monday–Friday & Saturday–Sunday). */
extern const ScheduleEntry_t default_schedule[SCHEDULE_ENTRIES];

/* ============================================================================
 *   FORBIDDEN WATERING PERIOD
 * ============================================================================ */
#define FORBIDDEN_START_HOUR  10U
#define FORBIDDEN_END_HOUR    15U

/* ============================================================================
 *   GLOBAL SYSTEM MODE (changeable via MQTT)
 * ============================================================================ */
extern ControlMode_t g_system_mode;

#ifdef __cplusplus
}
#endif

#endif /* APP_CONFIG_H */
