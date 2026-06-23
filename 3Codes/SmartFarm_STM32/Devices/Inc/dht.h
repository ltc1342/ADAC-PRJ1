/**
 * @file    dht.h
 * @brief   DHT11 / DHT22 (AM2302) temperature & humidity sensor driver (HAL)
 * @version 1.0
 * @date    2026-06-18
 *
 * @details
 *  Bit-banged driver for the single-wire, half-duplex DHT protocol shared
 *  by the DHT11 and DHT22/AM2302 sensors. The wire-level sequence (host
 *  start pulse, ~80 us sensor ACK, 40-bit data frame, 8-bit checksum) is
 *  identical for both parts -- only the start-pulse hold time and the
 *  data-field decoding differ, and both are handled internally based on
 *  DhtConfig_t::type.
 *
 *  All timing is measured with timer_delay.h, a free-running microsecond
 *  timer. timer_delay_init() must already have been called once (with any
 *  available timer) before dht_init()/dht_read() are used -- this driver
 *  does not own or configure a timer of its own, it only consumes
 *  timer_delay_us() / timer_delay_ms() / timer_delay_get_tick_us().
 *
 * @note    The data GPIO pin must be pre-configured by the user (CubeMX or
 *          manual HAL_GPIO_Init) as GPIO_MODE_OUTPUT_OD (open-drain) with
 *          a pull-up -- internal GPIO_PULLUP, or (recommended, especially
 *          for DHT22/AM2302) an external 4.7-10 kOhm resistor to VCC.
 *          Open-drain lets this driver both pull the line low (start
 *          signal) and read the sensor's response without ever
 *          reconfiguring the pin mode at runtime.
 *
 * @warning dht_read() is a blocking call (roughly 4-6 ms worst case) that
 *          busy-polls the data pin with microsecond timing. Call it from
 *          normal task / main-loop context only, never from an ISR. For
 *          best reliability avoid running long ISRs while a read is in
 *          progress: interrupt latency eats directly into the ~25-45 us
 *          margin between a logic-0 (~27 us high) and logic-1 (~70 us
 *          high) data bit.
 *
 * @warning Respect the datasheet-mandated minimum interval between
 *          samples (1 s for DHT11, 2 s for DHT22/AM2302). Sampling sooner
 *          returns DHT_ERR_NOT_READY without touching the bus -- see
 *          dht_is_ready() / dht_get_min_interval_ms().
 *
 * Example:
 * @code
 *   DhtConfig_t cfg = {
 *       .data = { .port = GPIOA, .pin = GPIO_PIN_1 },
 *       .type = DHT_TYPE_DHT22,
 *   };
 *   DhtHandle_t hdht;
 *   dht_init(&hdht, &cfg);
 *
 *   DhtReading_t reading;
 *   if (dht_read(&hdht, &reading) == DHT_OK) {
 *       float humidity_pct  = (float)reading.humidity_x10    / 10.0f;
 *       float temperature_c = (float)reading.temperature_x10 / 10.0f;
 *   }
 * @endcode
 */

#ifndef DHT_H
#define DHT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/*==============================================================================
 * COMPILE-TIME CONSTANTS
 *============================================================================*/

#define DHT_DATA_BYTES              (5U)     /**< RH_H, RH_L, T_H, T_L, CHECKSUM     */
#define DHT_DATA_BITS               (40U)    /**< DHT_DATA_BYTES * 8 bits             */

#define DHT_MIN_INTERVAL_DHT11_MS   (1000U)  /**< Datasheet: >= 1 s between samples   */
#define DHT_MIN_INTERVAL_DHT22_MS   (2000U)  /**< Datasheet: >= 2 s between samples   */

/*==============================================================================
 * ENUMERATIONS
 *============================================================================*/

/**
 * @brief Sensor variant -- selects start-pulse timing and data decoding.
 */
typedef enum
{
    DHT_TYPE_DHT11 = 0,  /**< DHT11 / DHT12: 8-bit integer + 8-bit decimal fields */
    DHT_TYPE_DHT22 = 1   /**< DHT22 / AM2302 / AM2301: 16-bit, fixed-point x10    */
} DhtType_t;

/**
 * @brief API return codes.
 */
typedef enum
{
    DHT_OK            = 0,  /**< Operation successful, reading valid               */
    DHT_ERR_NULL      = 1,  /**< Null pointer passed                               */
    DHT_ERR_NOT_INIT  = 2,  /**< Handle was never successfully initialised         */
    DHT_ERR_NOT_READY = 3,  /**< Called before the minimum sample interval         */
    DHT_ERR_TIMEOUT   = 4,  /**< Sensor did not respond (wiring / pull-up / absent) */
    DHT_ERR_CHECKSUM  = 5   /**< Frame received but checksum did not match         */
} DhtStatus_t;

/*==============================================================================
 * STRUCTURES
 *============================================================================*/

/**
 * @brief Single GPIO pin descriptor.
 */
typedef struct
{
    GPIO_TypeDef *port;  /**< GPIO port, e.g. GPIOA           */
    uint16_t      pin;   /**< GPIO pin mask, e.g. GPIO_PIN_0   */
} DhtPinDef_t;

/**
 * @brief Hardware configuration for one sensor.
 */
typedef struct
{
    DhtPinDef_t data;  /**< Single-wire data pin. Mandatory; must already be
                             configured as Output Open-Drain + pull-up.       */
    DhtType_t   type;  /**< DHT_TYPE_DHT11 or DHT_TYPE_DHT22                  */
} DhtConfig_t;

/**
 * @brief Decoded sensor reading, fixed-point (value x10, i.e. in tenths).
 *
 * E.g. humidity_x10 == 655 means 65.5 %RH; temperature_x10 == -55 means
 * -5.5 degrees Celsius.
 */
typedef struct
{
    int16_t humidity_x10;     /**< Relative humidity, tenths of a percent   */
    int16_t temperature_x10;  /**< Temperature, tenths of a degree Celsius  */
} DhtReading_t;

/**
 * @brief Driver handle -- declare one instance per physical sensor.
 *
 * Treat all fields except 'cfg' as opaque -- do not write to them directly.
 */
typedef struct
{
    DhtConfig_t cfg;          /**< Copy of user configuration                  */
    uint64_t    last_read_us; /**< timer_delay tick of the last sample attempt */
    uint8_t     has_sampled;  /**< Non-zero once a first sample has run        */
    uint8_t     ready;        /**< Non-zero after successful dht_init()        */
} DhtHandle_t;

/*==============================================================================
 * PUBLIC API
 *============================================================================*/

/**
 * @brief  Initialise a DHT11/DHT22 driver instance.
 * @param  handle  Pointer to an uninitialised DhtHandle_t.
 * @param  cfg     Pointer to a filled DhtConfig_t (copied internally).
 * @retval DHT_OK on success, DHT_ERR_NULL if either pointer is NULL.
 * @note   Does not touch the GPIO bus; the line is left exactly as the
 *         user configured it (idle high, held there by the pull-up).
 */
DhtStatus_t dht_init(DhtHandle_t *handle, const DhtConfig_t *cfg);

/**
 * @brief  Get the datasheet-mandated minimum interval between samples.
 * @param  type  DHT_TYPE_DHT11 or DHT_TYPE_DHT22.
 * @retval Minimum interval in milliseconds.
 */
uint32_t dht_get_min_interval_ms(DhtType_t type);

/**
 * @brief  Query whether the minimum inter-sample interval has elapsed.
 * @param  handle  Initialised handle (NULL or not-ready returns false).
 * @retval true if dht_read() may be called now, false if it would
 *         immediately return DHT_ERR_NOT_INIT or DHT_ERR_NOT_READY.
 */
bool dht_is_ready(const DhtHandle_t *handle);

/**
 * @brief  Perform one blocking read of humidity + temperature.
 *
 * @param  handle       Initialised handle.
 * @param  reading_out  Filled with the decoded reading on DHT_OK only;
 *                       left unmodified on any error.
 *
 * @retval DHT_OK             Reading captured and checksum valid.
 * @retval DHT_ERR_NULL       handle or reading_out is NULL.
 * @retval DHT_ERR_NOT_INIT   handle was never successfully initialised.
 * @retval DHT_ERR_NOT_READY  Called before the minimum sample interval.
 * @retval DHT_ERR_TIMEOUT    Sensor did not respond (wiring/pull-up/absent).
 * @retval DHT_ERR_CHECKSUM   Frame received but checksum did not match.
 *
 * @note   Blocking: roughly 4 ms (DHT11) to 6 ms (DHT22) worst case.
 * @note   Not reentrant for the SAME handle. Separate handles (separate
 *         sensors on separate pins) may be read independently.
 */
DhtStatus_t dht_read(DhtHandle_t *handle, DhtReading_t *reading_out);

#ifdef __cplusplus
}
#endif

#endif /* DHT_H */
