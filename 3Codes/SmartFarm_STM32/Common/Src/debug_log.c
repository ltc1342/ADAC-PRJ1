/**
 * @file    debug_log.c
 * @brief   Implementation of debug logging (SWO or UART).
 * @author  Group SmartFarm
 * @date    2026-07-01
 */

#include "debug_log.h"
#include "app_defs.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "stm32f4xx_hal.h"

#if (defined(DEBUG_UART_ENABLE) && (DEBUG_UART_ENABLE == 1U))
#include "uart.h"
#endif

/* Internal buffer for formatting */
static char log_buffer[UART_TX_BUFFER_SIZE];

/**
 * @brief   Low‑level character output (redirects to SWO or UART).
 * @param   ch  Character to send.
 * @return  The character sent (or -1 on error).
 */
static int debug_putchar(int ch) {
#if (defined(DEBUG_SWO_ENABLE) && (DEBUG_SWO_ENABLE == 1U))
    /* ITM_SendChar is defined in core_cm4.h (included via stm32f4xx_hal.h) */
    ITM_SendChar((uint32_t)ch);
    return ch;
#elif (defined(DEBUG_UART_ENABLE) && (DEBUG_UART_ENABLE == 1U))
    uint8_t byte = (uint8_t)ch;
    HAL_StatusTypeDef status = HAL_UART_Transmit(&huart1, &byte, 1U, 10U);
    (void)status;
    return ch;
#else
    /* No debug output selected – fallback: do nothing */
    return ch;
#endif
}

/**
 * @brief  Initialise the debug output.
 * @note   No specific initialisation is needed for SWO; UART is assumed
 *         to have been initialised earlier (by MX_USART1_UART_Init()).
 */
void debug_log_init(void) {
    /* Nothing to do */
}

/**
 * @brief  Send a formatted log message.
 * @param  format  printf‑style format string.
 * @param  ...     Variable arguments.
 */
void debug_log(const char *format, ...) {
    if (format == NULL) {
        return;
    }

    va_list args;
    va_start(args, format);
    int len = vsnprintf(log_buffer, sizeof(log_buffer), format, args);
    va_end(args);

    if (len > 0) {
        if ((size_t)len >= sizeof(log_buffer)) {
            log_buffer[sizeof(log_buffer) - 1U] = '\0';
        }
        for (size_t i = 0U; i < strlen(log_buffer); ++i) {
            (void)debug_putchar((int)log_buffer[i]);
        }
    }
}

/**
 * @brief  Dump a binary buffer as hexadecimal text.
 * @param  data  Pointer to the buffer.
 * @param  len   Number of bytes to dump.
 */
void debug_log_hex(const uint8_t *data, uint16_t len) {
    if (data == NULL) {
        debug_log("ERR: NULL pointer in debug_log_hex\r\n");
        return;
    }

    for (uint16_t i = 0U; i < len; ++i) {
        if ((i & 0x0FU) == 0U) {
            debug_log("\r\n");
        }
        char byte_str[4U];
        snprintf(byte_str, sizeof(byte_str), "%02X ", data[i]);
        for (size_t j = 0U; j < strlen(byte_str); ++j) {
            (void)debug_putchar((int)byte_str[j]);
        }
    }
    debug_log("\r\n");
}
