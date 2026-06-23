/**
 * @file app_config.h
 * @brief Centralised compile-time configuration for the Smart Farm
 *        ESP32-WROOM-32 gateway.
 *
 * Edit ONLY this file to change Wi-Fi credentials, MQTT broker, pin
 * assignments, task stack sizes, and power-management thresholds.
 * Every other module includes this header — there are no magic numbers
 * scattered in .c files.
 *
 * Target framework : ESP-IDF v5.x
 * Target hardware  : ESP32-WROOM-32 (ESP32-D0WDQ6, 4 MB flash)
 */

#pragma once

/* ── Wi-Fi ─────────────────────────────────────────────────────────────────
 * Change to your local access-point credentials.
 * For production, store these in NVS or use provisioning (SmartConfig).
 * ───────────────────────────────────────────────────────────────────────── */
#define WIFI_SSID               "iPhone 14"
#define WIFI_PASS               "BLiZ1342k"
#define WIFI_RECONNECT_MAX      5       /**< Max reconnect attempts          */
#define WIFI_RECONNECT_DELAY_MS 3000    /**< Delay between retries (ms)      */

/* ── MQTT Broker ───────────────────────────────────────────────────────────
 * Public test broker: broker.emqx.io (no auth needed, shared namespace).
 * For production: replace with local Mosquitto or HiveMQ Cloud.
 * ───────────────────────────────────────────────────────────────────────── */
#define MQTT_BROKER_URI         "mqtt://broker.emqx.io:1883"
#define MQTT_CLIENT_ID          "mqtt-explorer-59d94241"
#define MQTT_KEEPALIVE_SEC      60
#define MQTT_QOS_0              0
#define MQTT_QOS_1              1

/* ── MQTT Publish Topics ───────────────────────────────────────────────── */
#define TOPIC_TEMP              "farm/temperature"
#define TOPIC_HUM               "farm/humidity"
#define TOPIC_LIGHT             "farm/light"
#define TOPIC_SOIL              "farm/soil_moisture"
#define TOPIC_PUMP_STAT         "farm/pump_status"
#define TOPIC_MIST_STAT         "farm/mist_status"
#define TOPIC_SENSORS_JSON      "farm/sensors"      /**< Combined JSON payload */
#define TOPIC_STATUS            "farm/status"       /**< Heartbeat / LWT      */

/* ── MQTT Subscribe Topics (commands FROM broker TO STM32) ────────────── */
#define TOPIC_CMD_PUMP          "farm/commands/pump"
#define TOPIC_CMD_MIST          "farm/commands/mist"
#define TOPIC_CMD_MODE          "farm/commands/mode"
#define TOPIC_CMD_ACK           "farm/commands/ack"

/* ── UART2 (ESP32 ↔ STM32) ─────────────────────────────────────────────── */
#define STM32_UART_PORT         UART_NUM_2  /**< UART0 = monitor; use UART2  */
#define STM32_UART_TX_PIN       17          /**< GPIO17 → STM32 PA3 (RX)    */
#define STM32_UART_RX_PIN       16          /**< GPIO16 ← STM32 PA2 (TX)    */
#define STM32_UART_BAUD         115200
#define STM32_UART_RX_BUF       512         /**< Ring-buffer size (bytes)    */
#define STM32_UART_TX_BUF       256
#define STM32_UART_EVT_QUEUE    10          /**< UART event queue depth      */
#define UART_READ_TIMEOUT_MS    100

/* ── FreeRTOS Task Parameters ──────────────────────────────────────────── */
#define TASK_UART_STACK_SIZE    3072
#define TASK_UART_PRIORITY      5           /**< Higher than MQTT/Web tasks  */
#define TASK_UART_CORE          1           /**< Pin UART task to core 1     */

#define TASK_HEARTBEAT_STACK    2048
#define TASK_HEARTBEAT_PRIORITY 3
#define HEARTBEAT_PERIOD_MS     30000       /**< 30-second MQTT heartbeat    */

/* ── HTTP Server (Level 3) ─────────────────────────────────────────────── */
#define HTTP_SERVER_PORT        80
#define HTTP_SEND_TIMEOUT_SEC   5
#define HTTP_MAX_OPEN_SOCKETS   4

/* ── Power Management (Level 4) ────────────────────────────────────────── */
#define SLEEP_TIMER_US          (10ULL * 1000000ULL)  /**< 10 s sleep period */
#define ACTIVE_WINDOW_MS        300    /**< Max active time before sleeping  */

/**
 * @brief Set to 1 to enable Level-4 deep-sleep one-shot mode.
 *
 * In deep-sleep mode:
 *   app_main → connect → wait one UART frame → publish → deep_sleep (loop)
 * In normal mode (0):
 *   app_main → connect → continuous publish loop + HTTP server.
 */
#define ENABLE_DEEP_SLEEP       0
