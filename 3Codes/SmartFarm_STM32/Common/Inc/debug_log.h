/**
 * @file    debug_log.h
 * @brief   Debug logging interface (SWO or UART).
 * @author  Group SmartFarm
 * @date    2026-07-01
 *
 * @note    Set DEBUG_SWO_ENABLE or DEBUG_UART_ENABLE in app_defs.h
 *          to choose the output channel.
 */

#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#ifndef DEBUG_MODE_ENABLE
#define DEBUG_MODE_ENABLE    1

#endif

/** @brief Initialise debug output (if needed). */
void debug_log_init(void);

#if DEBUG_MODE_ENABLE

/** @brief Send formatted log message (printf‑style). */
void debug_log(const char *format, ...);

/** @brief Dump binary data as hex (for debugging). */
void debug_log_hex(const uint8_t *data, uint16_t len);

#else

#define debug_log(...) 		((void)0)
#define debug_log_hex(...) 	((void)0)

#endif

#ifdef __cplusplus
}
#endif

#endif /* DEBUG_LOG_H */
