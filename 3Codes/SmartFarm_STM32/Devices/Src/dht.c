/**
 * @file    dht.c
 * @brief   DHT11 / DHT22 (AM2302) temperature & humidity sensor driver (HAL)
 * @version 1.0
 * @date    2026-06-18
 *
 * @details
 *  Implements the bit-banged single-wire DHT protocol described in dht.h.
 *  Internal (static) helpers are only ever called with addresses taken
 *  from a caller-owned DhtHandle_t/DhtConfig_t that has already been
 *  validated at the public API boundary (dht_init/dht_read/dht_is_ready),
 *  so they intentionally skip redundant NULL re-checks on the hot,
 *  microsecond-timed bit-capture path.
 */

#include "dht.h"
#include "timing.h"

/*==============================================================================
 * PRIVATE CONSTANTS
 *============================================================================*/

/** Host start pulse hold time -- DHT11 datasheet minimum is 18 ms. */
#define DHT_START_LOW_DHT11_MS      (18U)

/** Host start pulse hold time -- DHT22/AM2302 datasheet minimum is 1 ms. */
#define DHT_START_LOW_DHT22_MS      (1U)

/** Time the host keeps the line released before sampling the sensor ACK. */
#define DHT_START_RELEASE_US        (30U)

/** Generous timeout for each handshake/bit edge; real edges are <= 90 us. */
#define DHT_EDGE_TIMEOUT_US         (150U)

/** Data-bit '0' vs '1' decision threshold (datasheet: ~27 us vs ~70 us). */
#define DHT_BIT_THRESHOLD_US        (40U)

/*==============================================================================
 * PRIVATE FUNCTION PROTOTYPES
 *============================================================================*/

static void dht_set_pin_low(const DhtPinDef_t *pin);
static void dht_set_pin_high(const DhtPinDef_t *pin);
static GPIO_PinState dht_read_pin(const DhtPinDef_t *pin);

static void dht_enter_critical(void);
static void dht_exit_critical(void);

static void dht_send_start_signal(const DhtHandle_t *handle);
static DhtStatus_t dht_wait_for_level(const DhtPinDef_t *pin,
                                       GPIO_PinState level,
                                       uint32_t timeout_us);
static DhtStatus_t dht_measure_high_us(const DhtPinDef_t *pin,
                                        uint32_t timeout_us,
                                        uint32_t *duration_us_out);
static DhtStatus_t dht_wait_for_sensor_response(const DhtPinDef_t *pin);
static DhtStatus_t dht_capture_frame(const DhtPinDef_t *pin,
                                      uint8_t data_out[DHT_DATA_BYTES]);

static bool dht_checksum_ok(const uint8_t data[DHT_DATA_BYTES]);
static void dht_decode_dht11(const uint8_t data[DHT_DATA_BYTES],
                              DhtReading_t *reading_out);
static void dht_decode_dht22(const uint8_t data[DHT_DATA_BYTES],
                              DhtReading_t *reading_out);
static void dht_mark_sampled(DhtHandle_t *handle);

/*==============================================================================
 * PUBLIC API
 *============================================================================*/

DhtStatus_t dht_init(DhtHandle_t *handle, const DhtConfig_t *cfg)
{
    if ((handle == NULL) || (cfg == NULL))
    {
        return DHT_ERR_NULL;
    }

    /* A NULL port would silently fault every HAL_GPIO_* call later on;
     * reject it now while we can still report a clean error code. */
    if (cfg->data.port == NULL)
    {
        return DHT_ERR_NULL;
    }

    handle->cfg          = *cfg;
    handle->last_read_us = 0U;
    handle->has_sampled  = 0U;
    handle->ready        = 1U;

    return DHT_OK;
}

uint32_t dht_get_min_interval_ms(DhtType_t type)
{
    return (type == DHT_TYPE_DHT11) ? DHT_MIN_INTERVAL_DHT11_MS
                                     : DHT_MIN_INTERVAL_DHT22_MS;
}

bool dht_is_ready(const DhtHandle_t *handle)
{
    uint64_t now_us;
    uint64_t elapsed_us;
    uint64_t min_interval_us;

    if ((handle == NULL) || (handle->ready == 0U))
    {
        return false;
    }

    /* First-ever sample is always allowed; we have no prior timestamp
     * to compare against. */
    if (handle->has_sampled == 0U)
    {
        return true;
    }

    now_us          = timing_get_us();
    elapsed_us      = now_us - handle->last_read_us;
    min_interval_us = (uint64_t)dht_get_min_interval_ms(handle->cfg.type) * 1000U;

    return (elapsed_us >= min_interval_us);
}

DhtStatus_t dht_read(DhtHandle_t *handle, DhtReading_t *reading_out)
{
    uint8_t data[DHT_DATA_BYTES];
    DhtStatus_t status;

    if ((handle == NULL) || (reading_out == NULL))
    {
        return DHT_ERR_NULL;
    }

    if (handle->ready == 0U)
    {
        return DHT_ERR_NOT_INIT;
    }

    if (!dht_is_ready(handle))
    {
        return DHT_ERR_NOT_READY;
    }

    dht_send_start_signal(handle);

    /* IRQs stay off only across the handshake + 40-bit capture (worst
     * case a few ms) -- this is the only section where missing a single
     * ~27 us pulse edge would corrupt the whole reading. */
    dht_enter_critical();
    status = dht_wait_for_sensor_response(&handle->cfg.data);
    if (status == DHT_OK)
    {
        status = dht_capture_frame(&handle->cfg.data, data);
    }
    dht_exit_critical();

    dht_mark_sampled(handle);

    if (status != DHT_OK)
    {
        return status;
    }

    if (!dht_checksum_ok(data))
    {
        return DHT_ERR_CHECKSUM;
    }

    if (handle->cfg.type == DHT_TYPE_DHT11)
    {
        dht_decode_dht11(data, reading_out);
    }
    else
    {
        dht_decode_dht22(data, reading_out);
    }

    return DHT_OK;
}

/*==============================================================================
 * PRIVATE HELPERS -- GPIO ACCESS
 *============================================================================*/

/**
 * @brief Actively pull the open-drain data line low (start condition).
 */
static void dht_set_pin_low(const DhtPinDef_t *pin)
{
    HAL_GPIO_WritePin(pin->port, pin->pin, GPIO_PIN_RESET);
}

/**
 * @brief Release the open-drain data line so the external pull-up takes it
 *        high; this is how the host both "writes 1" and switches to
 *        listening for the sensor without ever changing GPIO mode.
 */
static void dht_set_pin_high(const DhtPinDef_t *pin)
{
    HAL_GPIO_WritePin(pin->port, pin->pin, GPIO_PIN_SET);
}

static GPIO_PinState dht_read_pin(const DhtPinDef_t *pin)
{
    return HAL_GPIO_ReadPin(pin->port, pin->pin);
}

/*==============================================================================
 * PRIVATE HELPERS -- CRITICAL SECTION
 *============================================================================*/

/**
 * @brief Disable IRQs to keep bit-edge timing jitter-free.
 * @note  Safe for a few ms even without the timer_delay overflow IRQ:
 *        the underlying 16-bit timer at 1 us/tick does not wrap for
 *        ~65 ms, well above one full DHT frame capture.
 */
static void dht_enter_critical(void)
{
    __disable_irq();
}

static void dht_exit_critical(void)
{
    __enable_irq();
}

/*==============================================================================
 * PRIVATE HELPERS -- WIRE PROTOCOL
 *============================================================================*/

/**
 * @brief Drive the host start condition and release the bus.
 */
static void dht_send_start_signal(const DhtHandle_t *handle)
{
    uint32_t start_low_ms;

    start_low_ms = (handle->cfg.type == DHT_TYPE_DHT11) ? DHT_START_LOW_DHT11_MS
                                                          : DHT_START_LOW_DHT22_MS;

    dht_set_pin_low(&handle->cfg.data);
    timing_delay_ms(start_low_ms);

    dht_set_pin_high(&handle->cfg.data);
    timing_delay_us(DHT_START_RELEASE_US);
}

/**
 * @brief Busy-poll until the data pin reaches 'level' or 'timeout_us' lapses.
 */
static DhtStatus_t dht_wait_for_level(const DhtPinDef_t *pin,
                                       GPIO_PinState level,
                                       uint32_t timeout_us)
{
    uint64_t start_tick;
    uint64_t elapsed_us;

    start_tick = timing_get_us();

    while (dht_read_pin(pin) != level)
    {
        elapsed_us = timing_get_us() - start_tick;
        if (elapsed_us > (uint64_t)timeout_us)
        {
            return DHT_ERR_TIMEOUT;
        }
    }

    return DHT_OK;
}

/**
 * @brief Measure how long the pin stays high, starting from "now".
 * @note  Caller must already know the pin is currently high (typically
 *        right after dht_wait_for_level(..., GPIO_PIN_SET, ...)) so the
 *        measured duration corresponds to one data-bit pulse.
 */
static DhtStatus_t dht_measure_high_us(const DhtPinDef_t *pin,
                                        uint32_t timeout_us,
                                        uint32_t *duration_us_out)
{
    uint64_t start_tick;
    uint64_t elapsed_us;

    start_tick = timing_get_us();

    while (dht_read_pin(pin) == GPIO_PIN_SET)
    {
        elapsed_us = timing_get_us() - start_tick;
        if (elapsed_us > (uint64_t)timeout_us)
        {
            return DHT_ERR_TIMEOUT;
        }
    }

    *duration_us_out = (uint32_t)(timing_get_us() - start_tick);

    return DHT_OK;
}

/**
 * @brief Wait through the sensor's fixed ACK sequence:
 *        ~80 us low, then ~80 us high, right before bit 0 of the frame.
 */
static DhtStatus_t dht_wait_for_sensor_response(const DhtPinDef_t *pin)
{
    DhtStatus_t status;

    status = dht_wait_for_level(pin, GPIO_PIN_RESET, DHT_EDGE_TIMEOUT_US);
    if (status != DHT_OK)
    {
        return status;
    }

    status = dht_wait_for_level(pin, GPIO_PIN_SET, DHT_EDGE_TIMEOUT_US);
    if (status != DHT_OK)
    {
        return status;
    }

    return dht_wait_for_level(pin, GPIO_PIN_RESET, DHT_EDGE_TIMEOUT_US);
}

/**
 * @brief Capture the 40-bit (5-byte) data frame following the ACK.
 *
 * Each bit is: ~50 us low (start-of-bit) followed by a high pulse whose
 * length encodes the value -- ~27 us for '0', ~70 us for '1'.
 */
static DhtStatus_t dht_capture_frame(const DhtPinDef_t *pin,
                                      uint8_t data_out[DHT_DATA_BYTES])
{
    uint32_t bit_index;
    uint32_t byte_index;
    uint32_t duration_us;
    DhtStatus_t status;

    for (byte_index = 0U; byte_index < DHT_DATA_BYTES; byte_index++)
    {
        data_out[byte_index] = 0U;
    }

    for (bit_index = 0U; bit_index < DHT_DATA_BITS; bit_index++)
    {
        status = dht_wait_for_level(pin, GPIO_PIN_RESET, DHT_EDGE_TIMEOUT_US);
        if (status != DHT_OK)
        {
            return status;
        }

        status = dht_wait_for_level(pin, GPIO_PIN_SET, DHT_EDGE_TIMEOUT_US);
        if (status != DHT_OK)
        {
            return status;
        }

        status = dht_measure_high_us(pin, DHT_EDGE_TIMEOUT_US, &duration_us);
        if (status != DHT_OK)
        {
            return status;
        }

        byte_index = bit_index >> 3U;
        data_out[byte_index] = (uint8_t)(data_out[byte_index] << 1U);
        if (duration_us > DHT_BIT_THRESHOLD_US)
        {
            data_out[byte_index] |= 1U;
        }
    }

    return DHT_OK;
}

/*==============================================================================
 * PRIVATE HELPERS -- DECODING
 *============================================================================*/

static bool dht_checksum_ok(const uint8_t data[DHT_DATA_BYTES])
{
    uint8_t sum;

    sum = (uint8_t)(data[0] + data[1] + data[2] + data[3]);

    return (sum == data[4]);
}

/**
 * @brief Decode a DHT11 frame.
 * @note  Classic DHT11 parts always report data[1] == data[3] == 0; the
 *        tenths fields are still added so the same decoder transparently
 *        supports DHT11-compatible clones that do populate them.
 */
static void dht_decode_dht11(const uint8_t data[DHT_DATA_BYTES],
                              DhtReading_t *reading_out)
{
    int16_t temp_magnitude_x10;

    reading_out->humidity_x10 = (int16_t)(((uint16_t)data[0] * 10U) + data[1]);

    temp_magnitude_x10 = (int16_t)((((uint16_t)data[2] & 0x7FU) * 10U) + data[3]);

    if ((data[2] & 0x80U) != 0U)
    {
        reading_out->temperature_x10 = (int16_t)(-temp_magnitude_x10);
    }
    else
    {
        reading_out->temperature_x10 = temp_magnitude_x10;
    }
}

/**
 * @brief Decode a DHT22/AM2302 frame (16-bit fields, already in tenths,
 *        MSB of the temperature field is the sign bit).
 */
static void dht_decode_dht22(const uint8_t data[DHT_DATA_BYTES],
                              DhtReading_t *reading_out)
{
    uint16_t humidity_raw;
    uint16_t temp_raw;

    humidity_raw = (uint16_t)(((uint16_t)data[0] << 8U) | (uint16_t)data[1]);
    reading_out->humidity_x10 = (int16_t)humidity_raw;

    temp_raw = (uint16_t)((((uint16_t)data[2] & 0x7FU) << 8U) | (uint16_t)data[3]);

    if ((data[2] & 0x80U) != 0U)
    {
        reading_out->temperature_x10 = (int16_t)(-(int16_t)temp_raw);
    }
    else
    {
        reading_out->temperature_x10 = (int16_t)temp_raw;
    }
}

/**
 * @brief Stamp the handle so dht_is_ready() enforces the next minimum gap,
 *        regardless of whether this attempt succeeded or timed out --
 *        retrying instantly after a failed transaction only makes a
 *        confused sensor worse.
 */
static void dht_mark_sampled(DhtHandle_t *handle)
{
    handle->last_read_us = timing_get_us();
    handle->has_sampled  = 1U;
}
