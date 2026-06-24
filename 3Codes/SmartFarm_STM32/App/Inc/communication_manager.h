/**
 * @file    communication_manager.h
 * @brief   UART communication service for STM32 ↔ ESP32 data exchange.
 * @author  ltc1342
 * @date    2026-07-01
 *
 * @note    TX – STM32 → ESP32 (sensor data, relay status, heartbeat).
 *          RX – ESP32 → STM32 (relay commands, mode changes, NTP time sync).
 *
 *          TX packet format (CSV, '\n' terminated):
 *            <uptime_s>,<temp>,<hum>,<lux>,<soil>,<soil_temp>,
 *            <pump>,<mist>,<mode>\n
 *          Example:
 *            1200,25.3,58.0,320.0,45.0,22.1,1,0,0\n
 *
 *          RX command format (ASCII keyword + optional argument + '\n'):
 *            PUMP_ON\n
 *            PUMP_OFF\n
 *            MIST_ON\n
 *            MIST_OFF\n
 *            AUTO_ENABLE\n
 *            AUTO_DISABLE\n
 *            SET_TIME,HH,MM,SS\n   (NTP sync from ESP32)
 *
 *          The UART DMA-RX interrupt must call
 *          comm_manager_rx_isr_callback() from the HAL callback:
 *          @code
 *            void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
 *                if (huart->Instance == USART2) {
 *                    comm_manager_rx_isr_callback(&g_comm_mgr);
 *                }
 *            }
 *          @endcode
 *
 *          Usage:
 *          @code
 *            extern UART_HandleTypeDef huart2;   // CubeMX
 *            static CommManager_t comm_mgr;
 *
 *            comm_manager_init(&comm_mgr, &huart2, on_command_received, NULL);
 *
 *            // In main loop (every SENSOR_READ_INTERVAL_MS):
 *            comm_manager_send_data(&comm_mgr, &sensor_data,
 *                                   &relay_status, current_mode);
 *
 *            // Commands arrive asynchronously via on_command_received callback.
 *          @endcode
 */

#ifndef COMMUNICATION_MANAGER_H_
#define COMMUNICATION_MANAGER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "app_types.h"
#include "app_defs.h"

/* ============================================================================
 *   COMMAND TYPE ENUMERATION
 * ============================================================================ */

/**
 * @brief  Commands that can be received from ESP32.
 */
typedef enum {
    CMD_NONE         = 0U,
    CMD_PUMP_ON      = 1U,
    CMD_PUMP_OFF     = 2U,
    CMD_MIST_ON      = 3U,
    CMD_MIST_OFF     = 4U,
    CMD_AUTO_ENABLE  = 5U,
    CMD_AUTO_DISABLE = 6U,
    CMD_SET_TIME     = 7U,   /**< Carries hour/minute/second in payload  */
    CMD_UNKNOWN      = 0xFFU
} CommandType_t;

/* ============================================================================
 *   PARSED COMMAND
 * ============================================================================ */

/**
 * @brief  Fully parsed inbound command with optional time payload.
 */
typedef struct {
    CommandType_t type;     /**< Identified command                 */
    uint8_t       hour;     /**< Valid when type == CMD_SET_TIME    */
    uint8_t       minute;   /**< Valid when type == CMD_SET_TIME    */
    uint8_t       second;   /**< Valid when type == CMD_SET_TIME    */
} ParsedCommand_t;

/* ============================================================================
 *   CALLBACK TYPE
 * ============================================================================ */

/**
 * @brief  User callback invoked from comm_manager_process() when a valid
 *         command has been fully received and parsed.
 * @param  cmd        Parsed command.
 * @param  user_data  Opaque pointer registered at init.
 */
typedef void (*command_callback_fn_t)(const ParsedCommand_t *cmd,
                                       void                  *user_data);

/* ============================================================================
 *   MANAGER INSTANCE
 * ============================================================================ */

/**
 * @brief  Communication manager state.
 * @note   huart stored as void* to avoid dragging stm32f4xx_hal_uart.h
 *         into all translation units.  The .c file casts to
 *         UART_HandleTypeDef*.
 */
typedef struct {
    void                  *huart;           /**< UART_HandleTypeDef* (injected)   */
    uint8_t                tx_buf[UART_TX_BUFFER_SIZE];
    uint8_t                rx_buf[UART_RX_BUFFER_SIZE];
    uint8_t                rx_line[UART_RX_BUFFER_SIZE]; /**< Assembled line      */
    uint16_t               rx_head;         /**< Write index in rx_line           */
    uint8_t                rx_byte;         /**< Single-byte DMA landing pad      */
    command_callback_fn_t  on_command;      /**< Called after parse success       */
    void                  *callback_data;   /**< Passed to on_command             */
    uint32_t               tx_count;        /**< Transmitted packet counter       */
    uint32_t               rx_error_count;  /**< Parse / frame error counter      */
    bool                   initialized;
} CommManager_t;

/* Global pointer set by comm_manager_init() for HAL callback integration */
extern CommManager_t *g_comm_manager_global;

/* ============================================================================
 *   API
 * ============================================================================ */

/**
 * @brief  Initialise communication manager and arm UART DMA-RX.
 * @param  mgr          Manager instance (must not be NULL).
 * @param  huart        Pointer to UART_HandleTypeDef (must not be NULL).
 * @param  on_command   Callback for parsed inbound commands (may be NULL).
 * @param  user_data    Opaque pointer forwarded to on_command.
 * @return ERR_NONE on success, ERR_UART_RX_FAIL if DMA-RX arm fails.
 */
ErrorCode_t comm_manager_init(CommManager_t         *mgr,
                               void                  *huart,
                               command_callback_fn_t  on_command,
                               void                  *user_data);

/**
 * @brief  Format and transmit a sensor + relay + mode packet over UART.
 * @param  mgr    Initialised manager.
 * @param  data   Sensor snapshot (may be NULL → fields sent as -1).
 * @param  relay  Relay status   (may be NULL → fields sent as -1).
 * @param  mode   Current ControlMode_t (encoded as integer).
 * @return ERR_NONE on success, ERR_UART_TX_FAIL on HAL error.
 */
ErrorCode_t comm_manager_send_data(CommManager_t      *mgr,
                                    const SensorData_t *data,
                                    const RelayStatus_t *relay,
                                    ControlMode_t        mode);

/**
 * @brief  Send a plain heartbeat packet (uptime only) to confirm link.
 * @param  mgr       Initialised manager.
 * @param  uptime_s  System uptime in seconds.
 * @return ERR_NONE on success.
 */
ErrorCode_t comm_manager_send_heartbeat(CommManager_t *mgr,
                                         uint32_t       uptime_s);

/**
 * @brief  Process the RX buffer: assemble lines and invoke on_command.
 * @param  mgr  Initialised manager.
 * @note   Call from the main loop (NOT from ISR context).
 *         Returns immediately if no complete line is available yet.
 */
void comm_manager_process(CommManager_t *mgr);

/**
 * @brief  ISR-safe callback – call from HAL_UART_RxCpltCallback.
 * @param  mgr  Initialised manager.
 * @note   Copies one received byte into rx_line buffer and re-arms DMA.
 */
void comm_manager_rx_isr_callback(CommManager_t *mgr);

/**
 * @brief  Feed received bytes into the communication manager from DMA/IDLE.
 * @param  mgr  Initialised manager.
 * @param  data Buffer of received bytes.
 * @param  len  Number of bytes.
 */
void comm_manager_feed_rx_bytes(CommManager_t *mgr, const uint8_t *data, uint16_t len);

/**
 * @brief  Return cumulative RX parse-error count since init.
 * @param  mgr  Initialised manager.
 * @return Error count (0 on NULL mgr).
 */
uint32_t comm_manager_get_rx_error_count(const CommManager_t *mgr);

/**
 * @brief  Return cumulative TX packet count since init.
 * @param  mgr  Initialised manager.
 * @return Packet count (0 on NULL mgr).
 */
uint32_t comm_manager_get_tx_count(const CommManager_t *mgr);



#ifdef __cplusplus
}
#endif

#endif /* COMMUNICATION_MANAGER_H_ */
