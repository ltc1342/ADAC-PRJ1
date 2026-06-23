/**
 * @file    communication_manager.c
 * @brief   UART communication service implementation.
 * @author  Group SmartFarm
 * @date    2026-07-01
 */

#include "communication_manager.h"
#include "debug_log.h"
#include "timing.h"
#include <string.h>
#include <stdio.h>

/* HAL includes */
#include "stm32f4xx_hal.h"

/* Global pointer to the active communication manager instance.
    This allows HAL callbacks (in other translation units) to notify the
    manager without requiring the user to pass the pointer around. */
CommManager_t *g_comm_manager_global = NULL;

/* ============================================================================
 *   PRIVATE HELPERS
 * ============================================================================ */

static UART_HandleTypeDef* get_huart(CommManager_t *mgr)
{
    return (UART_HandleTypeDef*)mgr->huart;
}

static void reset_rx_line(CommManager_t *mgr)
{
    mgr->rx_head = 0U;
    mgr->rx_line[0] = '\0';
}

static void process_line(CommManager_t *mgr)
{
    if (mgr->rx_head == 0U)
    {
        return;
    }

    /* Null-terminate */
    mgr->rx_line[mgr->rx_head] = '\0';

    /* Parse command */
    ParsedCommand_t cmd;
    cmd.type = CMD_UNKNOWN;
    cmd.hour = cmd.minute = cmd.second = 0U;

    if (strncmp((char*)mgr->rx_line, "PUMP_ON", 7) == 0)
        cmd.type = CMD_PUMP_ON;
    else if (strncmp((char*)mgr->rx_line, "PUMP_OFF", 8) == 0)
        cmd.type = CMD_PUMP_OFF;
    else if (strncmp((char*)mgr->rx_line, "MIST_ON", 7) == 0)
        cmd.type = CMD_MIST_ON;
    else if (strncmp((char*)mgr->rx_line, "MIST_OFF", 8) == 0)
        cmd.type = CMD_MIST_OFF;
    else if (strncmp((char*)mgr->rx_line, "AUTO_ENABLE", 11) == 0)
        cmd.type = CMD_AUTO_ENABLE;
    else if (strncmp((char*)mgr->rx_line, "AUTO_DISABLE", 12) == 0)
        cmd.type = CMD_AUTO_DISABLE;
    else if (strncmp((char*)mgr->rx_line, "SET_TIME", 8) == 0)
    {
        /* Format: SET_TIME,HH,MM,SS */
        if (sscanf((char*)mgr->rx_line, "SET_TIME,%hhu,%hhu,%hhu",
                   &cmd.hour, &cmd.minute, &cmd.second) == 3)
        {
            cmd.type = CMD_SET_TIME;
        }
        else
        {
            cmd.type = CMD_UNKNOWN;
        }
    }

    /* Invoke callback if registered */
    if ((mgr->on_command != NULL) && (cmd.type != CMD_UNKNOWN))
    {
        mgr->on_command(&cmd, mgr->callback_data);
    }
    else if (cmd.type == CMD_UNKNOWN)
    {
        mgr->rx_error_count++;
        debug_log("comm: unknown command: %s\r\n", mgr->rx_line);
    }

    /* Reset line */
    reset_rx_line(mgr);
}

/* ============================================================================
 *   PUBLIC API
 * ============================================================================ */

ErrorCode_t comm_manager_init(CommManager_t         *mgr,
                               void                  *huart,
                               command_callback_fn_t  on_command,
                               void                  *user_data)
{
    if ((mgr == NULL) || (huart == NULL))
    {
        return ERR_INVALID_PARAM;
    }

    mgr->huart = huart;
    mgr->on_command = on_command;
    mgr->callback_data = user_data;
    mgr->tx_count = 0U;
    mgr->rx_error_count = 0U;
    mgr->initialized = true;

    /* Initialise RX buffers */
    reset_rx_line(mgr);
    mgr->rx_byte = 0U;

    /* Arm DMA-RX (assuming idle-line detection or single-byte interrupt) */
    /* Here we use single-byte interrupt method: enable RXNE interrupt */
    __HAL_UART_ENABLE_IT(get_huart(mgr), UART_IT_RXNE);

    /* Expose manager globally for HAL callback integration */
    g_comm_manager_global = mgr;

    debug_log("comm_manager: init OK\r\n");
    return ERR_NONE;
}

ErrorCode_t comm_manager_send_data(CommManager_t      *mgr,
                                    const SensorData_t *data,
                                    const RelayStatus_t *relay,
                                    ControlMode_t        mode)
{
    if ((mgr == NULL) || (!mgr->initialized))
    {
        return ERR_INVALID_PARAM;
    }

    char buf[UART_TX_BUFFER_SIZE];
    int len;

    /* Build CSV packet */
    if (data != NULL)
    {
        len = snprintf(buf, sizeof(buf),
                       "%lu,%.1f,%.1f,%.1f,%.1f,%.1f,%d,%d,%d\n",
                       timing_get_ms() / 1000U,
                       data->temperature,
                       data->humidity,
                       data->light_lux,
                       data->soil_moisture,
                       data->soil_temperature,
                       (relay != NULL) ? (int)relay->pump : -1,
                       (relay != NULL) ? (int)relay->mist : -1,
                       (int)mode);
    }
    else
    {
        len = snprintf(buf, sizeof(buf),
                       "%lu,-1,-1,-1,-1,-1,%d,%d,%d\n",
                       timing_get_ms() / 1000U,
                       (relay != NULL) ? (int)relay->pump : -1,
                       (relay != NULL) ? (int)relay->mist : -1,
                       (int)mode);
    }

    if ((len < 0) || ((size_t)len >= sizeof(buf)))
    {
        return ERR_INVALID_PARAM;
    }

    if (HAL_UART_Transmit(get_huart(mgr), (uint8_t*)buf, (uint16_t)len, 100U) != HAL_OK)
    {
        return ERR_UART_TX_FAIL;
    }

    mgr->tx_count++;
    return ERR_NONE;
}

ErrorCode_t comm_manager_send_heartbeat(CommManager_t *mgr,
                                         uint32_t       uptime_s)
{
    if ((mgr == NULL) || (!mgr->initialized))
    {
        return ERR_INVALID_PARAM;
    }

    char buf[32];
    int len = snprintf(buf, sizeof(buf), "HB,%lu\n", uptime_s);
    if ((len < 0) || ((size_t)len >= sizeof(buf)))
    {
        return ERR_INVALID_PARAM;
    }

    if (HAL_UART_Transmit(get_huart(mgr), (uint8_t*)buf, (uint16_t)len, 100U) != HAL_OK)
    {
        return ERR_UART_TX_FAIL;
    }

    return ERR_NONE;
}

void comm_manager_process(CommManager_t *mgr)
{
    if ((mgr == NULL) || (!mgr->initialized))
    {
        return;
    }

    /* Check if a complete line (newline) has been received */
    /* We rely on rx_head being incremented by ISR, and we check for '\n' */
    /* This function is called from main loop, not ISR */
    /* We need to protect against concurrent access; but since ISR only writes,
     * and we read, we can use a simple flag or check atomically.
     * For simplicity, we assume no concurrent modification during read. */
    if (mgr->rx_head > 0U)
    {
        /* Check if last char is '\n' */
        if (mgr->rx_line[mgr->rx_head - 1] == '\n')
        {
            process_line(mgr);
        }
    }
}

void comm_manager_rx_isr_callback(CommManager_t *mgr)
{
    if ((mgr == NULL) || (!mgr->initialized))
    {
        return;
    }

    uint8_t byte = mgr->rx_byte;   /* filled by HAL */

    /* Store into line buffer if space */
    if (mgr->rx_head < (UART_RX_BUFFER_SIZE - 1U))
    {
        mgr->rx_line[mgr->rx_head++] = byte;
    }
    else
    {
        /* Buffer overflow: reset line */
        reset_rx_line(mgr);
        mgr->rx_error_count++;
    }

    /* Re-arm RX interrupt (if using single-byte) */
    HAL_UART_Receive_IT(get_huart(mgr), &mgr->rx_byte, 1);
}

uint32_t comm_manager_get_tx_count(const CommManager_t *mgr)
{
    if ((mgr == NULL) || (!mgr->initialized))
    {
        return 0U;
    }
    return mgr->tx_count;
}

uint32_t comm_manager_get_rx_error_count(const CommManager_t *mgr)
{
    if ((mgr == NULL) || (!mgr->initialized))
    {
        return 0U;
    }
    return mgr->rx_error_count;
}
