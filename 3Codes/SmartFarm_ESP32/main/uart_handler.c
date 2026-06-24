/**
 * @file uart_handler.c
 * @brief UART2 interrupt-driven implementation.
 *
 * Key design decisions:
 *  1. uart_driver_install() with an event queue enables ISR-based
 *     reception — the CPU is NOT busy-polling.
 *  2. uart_enable_pattern_det_baud_intr() fires an extra event on '\n',
 *     ensuring the task wakes immediately when a full frame arrives even
 *     if the FIFO threshold was not reached.
 *  3. A local line_buf[] accumulates bytes across multiple UART events
 *     (frame can be split across multiple ISR triggers).
 *  4. Command strings are stored in a const table in flash (.rodata),
 *     saving RAM.
 *  5. The RX task is pinned to core 1 to keep core 0 free for
 *     Wi-Fi / MQTT / HTTP workloads.
 */

#include "uart_handler.h"
#include "sensor_store.h"
#include "app_config.h"

#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "UART_HANDLER";

/* ── Command string table (flash .rodata, not RAM) ───────────────────────── */
/** Maps farm_cmd_t enum values to the ASCII strings sent to STM32. */
static const char *const s_cmd_strings[CMD_UNKNOWN] = {
    [CMD_PUMP_ON]     = "PUMP_ON\n",
    [CMD_PUMP_OFF]    = "PUMP_OFF\n",
    [CMD_MIST_ON]     = "MIST_ON\n",
    [CMD_MIST_OFF]    = "MIST_OFF\n",
    [CMD_MODE_AUTO]   = "AUTO_ENABLE\n",
    [CMD_MODE_MANUAL] = "MANUAL_ENABLE\n",
};

/* UART event queue (populated by ISR, consumed by uart_rx_task) */
static QueueHandle_t s_uart_evt_q = NULL;

/* ── Internal: CSV parser ────────────────────────────────────────────────── */

/**
 * @brief Parse a null-terminated CSV line into a sensor_data_t.
 *
 * Expected format: "25.3,58,320,45,1,0"
 *                   temp hum light soil pump mist
 *
 * Uses sscanf with explicit field widths.  Returns false and marks
 * data->valid = false on any parse error so callers can skip bad frames.
 *
 * @param line  Null-terminated input (trailing \r\n already stripped).
 * @param out   Output struct; out->valid is set by this function.
 * @return true on success.
 */
static bool parse_sensor_line(const char *line, sensor_data_t *out)
{
    /* sscanf %hhu = unsigned char (uint8_t), %hu = unsigned short (uint16_t) */
    int n = sscanf(line, "%f,%f,%hu,%hhu,%hhu,%hhu",
                   &out->temperature,
                   &out->humidity,
                   &out->light,
                   &out->soil_moisture,
                   &out->pump_status,
                   &out->mist_status);

    if (n != 6) {
        ESP_LOGW(TAG, "Bad frame (%d/6 fields): \"%s\"", n, line);
        out->valid = false;
        return false;
    }

    /* Basic sanity checks */
    if (out->temperature < -40.0f || out->temperature > 85.0f ||
        out->humidity    < 0.0f   || out->humidity    > 100.0f) {
        ESP_LOGW(TAG, "Sensor values out of range, discarding");
        out->valid = false;
        return false;
    }

    out->valid = true;
    return true;
}

/* ── UART RX task ────────────────────────────────────────────────────────── */

/**
 * @brief FreeRTOS task that processes events from the UART ISR queue.
 *
 * State machine:
 *   IDLE → accumulate bytes into line_buf[]
 *   '\n' received → parse → sensor_store_set() → reset line_buf
 *   FIFO/buffer overflow → flush + reset
 *
 * Stack usage is bounded by the static line_buf[64] and rx_buf[128].
 * No heap allocation occurs inside the task loop.
 *
 * @param arg  Unused task parameter.
 */
static void uart_rx_task(void *arg)
{
    uart_event_t event;
    uint8_t      rx_buf[128];          /* Temporary read buffer  */
    char         line_buf[64];         /* Accumulation buffer    */
    size_t       line_len = 0;

    ESP_LOGI(TAG, "RX task started on core %d", xPortGetCoreID());

    for (;;) {
        /* Block indefinitely until the ISR posts an event */
        if (xQueueReceive(s_uart_evt_q, &event, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        switch (event.type) {

        /* ── New data available in the ring buffer ── */
        case UART_DATA:
        case UART_PATTERN_DET: {
            size_t avail = 0;
            uart_get_buffered_data_len(STM32_UART_PORT, &avail);
            if (avail == 0) break;

            /* Cap to our local rx_buf to avoid stack overflow */
            if (avail > sizeof(rx_buf)) avail = sizeof(rx_buf);

            int read = uart_read_bytes(STM32_UART_PORT, rx_buf,
                                       (uint32_t)avail,
                                       pdMS_TO_TICKS(UART_READ_TIMEOUT_MS));
            if (read <= 0) break;

            /* Accumulate bytes into line_buf, trigger parse on '\n' */
            for (int i = 0; i < read; i++) {
                char c = (char)rx_buf[i];

                if (c == '\n' || c == '\r') {
                    /* End-of-line: attempt parse if we have content */
                    if (line_len > 0) {
                        line_buf[line_len] = '\0';

                        sensor_data_t data = {0};
                        if (parse_sensor_line(line_buf, &data)) {
                            sensor_store_set(&data);
                            ESP_LOGD(TAG,
                                     "Frame OK → T=%.1f H=%.1f L=%u "
                                     "S=%u P=%u M=%u",
                                     data.temperature, data.humidity,
                                     data.light,       data.soil_moisture,
                                     data.pump_status, data.mist_status);
                        }
                        line_len = 0; /* Reset for next frame */
                    }
                } else {
                    /* Accumulate — guard against buffer overrun */
                    if (line_len < (sizeof(line_buf) - 1)) {
                        line_buf[line_len++] = c;
                    } else {
                        /* Frame too long — discard and re-sync */
                        ESP_LOGW(TAG, "Line buffer overrun, re-syncing");
                        line_len = 0;
                    }
                }
            }
            break;
        }

        /* ── Hardware FIFO overflowed — flush and re-sync ── */
        case UART_FIFO_OVF:
            ESP_LOGW(TAG, "FIFO overflow – flushing RX");
            uart_flush_input(STM32_UART_PORT);
            xQueueReset(s_uart_evt_q);
            line_len = 0;
            break;

        /* ── Ring buffer full — same recovery ── */
        case UART_BUFFER_FULL:
            ESP_LOGW(TAG, "Ring buffer full – flushing RX");
            uart_flush_input(STM32_UART_PORT);
            xQueueReset(s_uart_evt_q);
            line_len = 0;
            break;

        /* ── BREAK condition (STM32 reset / cable issue) ── */
        case UART_BREAK:
            ESP_LOGW(TAG, "UART BREAK detected – STM32 may have reset");
            line_len = 0;
            break;

        default:
            break;
        }
    }

    /* Should never reach here */
    vTaskDelete(NULL);
}

/* ── Public API ──────────────────────────────────────────────────────────── */

esp_err_t uart_handler_init(void)
{
    /* 1. Configure UART parameters (115200, 8N1, no flow control) */
    const uart_config_t uart_cfg = {
        .baud_rate           = STM32_UART_BAUD,
        .data_bits           = UART_DATA_8_BITS,
        .parity              = UART_PARITY_DISABLE,
        .stop_bits           = UART_STOP_BITS_1,
        .flow_ctrl           = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk          = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config(STM32_UART_PORT, &uart_cfg));

    /* 2. Assign GPIO pins */
    ESP_ERROR_CHECK(uart_set_pin(STM32_UART_PORT,
                                 STM32_UART_TX_PIN,   /* TX: GPIO17 */
                                 STM32_UART_RX_PIN,   /* RX: GPIO16 */
                                 UART_PIN_NO_CHANGE,  /* RTS: unused */
                                 UART_PIN_NO_CHANGE   /* CTS: unused */
                                 ));

    /* 3. Install driver with ISR event queue
     *    RX ring-buffer: STM32_UART_RX_BUF bytes
     *    TX ring-buffer: STM32_UART_TX_BUF bytes
     *    Event queue depth: STM32_UART_EVT_QUEUE entries */
    ESP_ERROR_CHECK(uart_driver_install(STM32_UART_PORT,
                                        STM32_UART_RX_BUF,
                                        STM32_UART_TX_BUF,
                                        STM32_UART_EVT_QUEUE,
                                        &s_uart_evt_q,
                                        0  /* No ESP_INTR_FLAG_IRAM needed */
                                        ));

    /* 4. Enable pattern detection interrupt on '\n'
     *    This fires UART_PATTERN_DET event immediately when '\n' is seen,
     *    rather than waiting for the FIFO threshold — reduces latency. */
    ESP_ERROR_CHECK(
        uart_enable_pattern_det_baud_intr(STM32_UART_PORT,
                                          '\n', /* pattern char   */
                                          1,    /* consecutive count */
                                          9,    /* gap max (baud)  */
                                          0,    /* pre-idle (baud) */
                                          0     /* post-idle       */
                                          ));
    uart_pattern_queue_reset(STM32_UART_PORT, 20);

    /* 5. Start the RX task pinned to core 1 */
    BaseType_t rc = xTaskCreatePinnedToCore(
        uart_rx_task,
        "uart_rx",
        TASK_UART_STACK_SIZE,
        NULL,
        TASK_UART_PRIORITY,
        NULL,
        TASK_UART_CORE
    );

    if (rc != pdPASS) {
        ESP_LOGE(TAG, "Failed to create uart_rx task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "UART2 ready: TX=GPIO%d RX=GPIO%d BAUD=%d",
             STM32_UART_TX_PIN, STM32_UART_RX_PIN, STM32_UART_BAUD);
    return ESP_OK;
}

int uart_handler_send(const char *data, size_t len)
{
    if (!data || len == 0) return -1;
    return uart_write_bytes(STM32_UART_PORT, data, len);
}

void uart_handler_send_cmd(farm_cmd_t cmd)
{
    if (cmd >= CMD_UNKNOWN) {
        ESP_LOGW(TAG, "uart_handler_send_cmd: invalid cmd %d", (int)cmd);
        return;
    }

    const char *str    = s_cmd_strings[cmd];
    size_t      str_len = strlen(str);
    int written = uart_handler_send(str, str_len);

    if (written == (int)str_len) {
        ESP_LOGI(TAG, "CMD→STM32: \"%.*s\"", (int)(str_len - 1), str);
    } else {
        ESP_LOGE(TAG, "UART write error (wrote %d/%d bytes)", written, (int)str_len);
    }
}
