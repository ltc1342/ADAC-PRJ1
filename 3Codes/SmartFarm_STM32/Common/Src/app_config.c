/**
 * @file    app_config.c
 * @brief   Definition of runtime configuration variables.
 * @author  Group SmartFarm
 * @date    2026-07-01
 */

#include "app_config.h"

/* Global system mode – default AUTO */
ControlMode_t g_system_mode = MODE_AUTO;

/* Default watering schedule: Monday–Friday at 6:00 & 17:00,
   Saturday–Sunday at 7:00 & 18:00, each 60 seconds duration. */
/* Note: ScheduleEntry_t contains a weekday_mask field (bit0=Mon .. bit6=Sun).
   Initialise masks to avoid undefined behaviour in schedule checks. */
const ScheduleEntry_t default_schedule[SCHEDULE_ENTRIES] = {
    { .hour = 6U,  .minute = 0U,  .duration_s = 60U, .weekday_mask = 0x1FU }, /* Mon–Fri */
    { .hour = 17U, .minute = 0U,  .duration_s = 60U, .weekday_mask = 0x1FU }, /* Mon–Fri */
    { .hour = 7U,  .minute = 0U,  .duration_s = 60U, .weekday_mask = 0x60U }, /* Sat–Sun */
    { .hour = 18U, .minute = 0U, .duration_s = 60U, .weekday_mask = 0x60U }  /* Sat–Sun */
};
