/**
 * @file uart_handler.h
 * @brief UART2 interrupt-driven driver for STM32 ↔ ESP32 communication.
 *
 * Architecture (interrupt-based, NOT polling):
 *
 *   [STM32 UART TX] ──► GPIO16 (RX2)
 *   [STM32 UART RX] ◄── GPIO17 (TX2)
 *
 *   Hardware UART FIFO fills → UART ISR fires → pushes uart_event_t
 *   into s_uart_evt_queue → uart_rx_task() wakes, reads bytes,
 *   accumulates into a line buffer, calls parse_sensor_line() on '\n',
 *   then calls sensor_store_set() with the parsed data.
 *
 * Wire protocol (STM32 → ESP32):
 *   ASCII CSV, newline-terminated:  "25.3,58,320,45,1,0\n"
 *   Fields: temp(°C), hum(%RH), light(lux), soil(%), pump(0/1), mist(0/1)
 *
 * Wire protocol (ESP32 → STM32):
 *   ASCII command string, newline-terminated:
 *   "PUMP_ON\n" | "PUMP_OFF\n" | "MIST_ON\n" | "MIST_OFF\n"
 *   "AUTO_ENABLE\n" | "MANUAL_ENABLE\n"
 */

#pragma once

#include "sensor_data.h"
#include "esp_err.h"
#include <stddef.h>

/**
 * @brief Initialise UART2 and start the interrupt-driven RX task.
 *
 * Configures: 115200 8N1, no hardware flow control, UART2 pins.
 * Installs the UART driver with an event ISR queue.
 * Spawns the uart_rx_task pinned to core 1.
 *
 * @return ESP_OK on success, ESP_FAIL if task creation fails.
 */
esp_err_t uart_handler_init(void);

/**
 * @brief Transmit raw bytes to STM32 over UART2.
 *
 * Thread-safe (uart_write_bytes is internally serialised by the driver).
 *
 * @param data  Pointer to data buffer.
 * @param len   Number of bytes to send.
 * @return      Number of bytes written, or -1 on error.
 */
int uart_handler_send(const char *data, size_t len);

/**
 * @brief Map a farm_cmd_t enum to its ASCII string and transmit to STM32.
 *
 * Looks up the string in a compile-time table (no heap allocation).
 *
 * @param cmd  Command to send. CMD_UNKNOWN is silently ignored.
 */
void uart_handler_send_cmd(farm_cmd_t cmd);
