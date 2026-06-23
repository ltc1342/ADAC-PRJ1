/**
 * @file    ds18b20.c
 * @brief   DS18B20 1-Wire driver implementation (LL GPIO + timing.h).
 * @author  ltc1342
 * @date    2026-07-01
 *
 * @note    1-Wire protocol timing per DS18B20 datasheet Rev 5.
 *          GPIO direction is toggled between OUTPUT (drive low) and
 *          INPUT (release – external 4.7 kΩ pull-up brings line high).
 */

#include "ds18b20.h"
#include "timing.h"
#include "stm32f4xx_ll_gpio.h"

/* ============================================================================
 *   1-WIRE ROM / FUNCTION COMMANDS
 * ============================================================================ */

#define OW_CMD_SKIP_ROM          0xCCU
#define OW_CMD_CONVERT_T         0x44U
#define OW_CMD_READ_SCRATCHPAD   0xBEU

/* ============================================================================
 *   1-WIRE TIMING (µs) – datasheet Table 1
 * ============================================================================ */

#define OW_RESET_HOLD_US         480U  /* Master holds LOW for reset        */
#define OW_RESET_RELEASE_US       70U  /* Master releases, then samples     */
#define OW_PRESENCE_WINDOW_US    410U  /* Remainder of reset window         */

#define OW_WRITE1_LOW_US           6U  /* Short LOW pulse for '1' bit       */
#define OW_WRITE1_RELEASE_US      64U  /* Release remainder of 70 µs slot   */
#define OW_WRITE0_LOW_US          60U  /* Long LOW pulse for '0' bit        */
#define OW_WRITE0_RELEASE_US       4U  /* Short recovery after '0'          */

#define OW_READ_INIT_US            6U  /* Master pulls LOW to start read    */
#define OW_READ_SAMPLE_US          9U  /* Release then sample at 9 µs mark  */
#define OW_READ_SLOT_TOTAL_US     70U  /* Total read slot length            */

/* ============================================================================
 *   PRIVATE: GPIO DIRECTION HELPERS
 * ============================================================================ */

/**
 * @brief  Drive data line LOW (switch to push-pull output, set 0).
 */
static void ow_drive_low(const Ds18b20Config_t *cfg)
{
    LL_GPIO_SetPinMode(cfg->port, cfg->pin, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinOutputType(cfg->port, cfg->pin, LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_ResetOutputPin(cfg->port, cfg->pin);
}

/**
 * @brief  Release data line (switch to input – pull-up brings line HIGH).
 */
static void ow_release(const Ds18b20Config_t *cfg)
{
    LL_GPIO_SetPinMode(cfg->port, cfg->pin, LL_GPIO_MODE_INPUT);
}

/**
 * @brief  Sample the current level of the data line.
 * @return 1 if HIGH, 0 if LOW.
 */
static uint8_t ow_sample(const Ds18b20Config_t *cfg)
{
    return (uint8_t)LL_GPIO_IsInputPinSet(cfg->port, cfg->pin);
}

/* ============================================================================
 *   PRIVATE: 1-WIRE PROTOCOL PRIMITIVES
 * ============================================================================ */

/**
 * @brief  Issue a reset pulse and detect presence.
 * @return SENSOR_OK if at least one device pulled line LOW,
 *         SENSOR_NOT_FOUND otherwise.
 */
static SensorError_t ow_reset(const Ds18b20Config_t *cfg)
{
    uint8_t presence;

    ow_drive_low(cfg);
    timing_delay_us(OW_RESET_HOLD_US);

    ow_release(cfg);
    timing_delay_us(OW_RESET_RELEASE_US);

    /* A device holding the line LOW = presence pulse */
    presence = ow_sample(cfg);

    /* Wait out the rest of the presence window */
    timing_delay_us(OW_PRESENCE_WINDOW_US);

    /* presence == 0 means device pulled line low → device found */
    return (presence == 0U) ? SENSOR_OK : SENSOR_NOT_FOUND;
}

/**
 * @brief  Write a single bit onto the bus (LSB-first caller responsibility).
 * @param  bit  0 or 1.
 */
static void ow_write_bit(const Ds18b20Config_t *cfg, uint8_t bit)
{
    if (bit != 0U) {
        /* Write '1': short LOW pulse then release */
        ow_drive_low(cfg);
        timing_delay_us(OW_WRITE1_LOW_US);
        ow_release(cfg);
        timing_delay_us(OW_WRITE1_RELEASE_US);
    } else {
        /* Write '0': hold LOW for full slot then release */
        ow_drive_low(cfg);
        timing_delay_us(OW_WRITE0_LOW_US);
        ow_release(cfg);
        timing_delay_us(OW_WRITE0_RELEASE_US);
    }
}

/**
 * @brief  Write one byte LSB-first.
 */
static void ow_write_byte(const Ds18b20Config_t *cfg, uint8_t byte)
{
    for (uint8_t i = 0U; i < 8U; i++) {
        ow_write_bit(cfg, (byte >> i) & 0x01U);
    }
}

/**
 * @brief  Read a single bit from the bus.
 * @return Bit value (0 or 1).
 */
static uint8_t ow_read_bit(const Ds18b20Config_t *cfg)
{
    uint8_t bit;

    /* Initiate read slot: pull LOW then release */
    ow_drive_low(cfg);
    timing_delay_us(OW_READ_INIT_US);
    ow_release(cfg);
    timing_delay_us(OW_READ_SAMPLE_US);

    bit = ow_sample(cfg);

    /* Wait out remaining slot time */
    timing_delay_us(OW_READ_SLOT_TOTAL_US - OW_READ_INIT_US - OW_READ_SAMPLE_US);

    return bit;
}

/**
 * @brief  Read one byte LSB-first.
 */
static uint8_t ow_read_byte(const Ds18b20Config_t *cfg)
{
    uint8_t byte = 0U;

    for (uint8_t i = 0U; i < 8U; i++) {
        if (ow_read_bit(cfg) != 0U) {
            byte |= (uint8_t)(1U << i);
        }
    }

    return byte;
}

/* ============================================================================
 *   PRIVATE: CRC-8 (DALLAS / MAXIM)
 *   Polynomial: x^8 + x^5 + x^4 + 1  (reflected form: 0x8C)
 * ============================================================================ */

static uint8_t compute_crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0U;

    for (uint8_t i = 0U; i < len; i++) {
        uint8_t byte = data[i];
        for (uint8_t b = 0U; b < 8U; b++) {
            uint8_t mix = (crc ^ byte) & 0x01U;
            crc >>= 1U;
            if (mix != 0U) {
                crc ^= 0x8CU;
            }
            byte >>= 1U;
        }
    }

    return crc;
}

/* ============================================================================
 *   PRIVATE: TEMPERATURE CONVERSION
 *   Scratchpad byte[0] = TempLSB, byte[1] = TempMSB (12-bit, two's comp.)
 *   Resolution: 1 LSB = 0.0625 °C
 * ============================================================================ */

static float convert_raw_to_celsius(const uint8_t *scratchpad)
{
    uint16_t raw_u  = (uint16_t)(((uint16_t)scratchpad[1U] << 8U) |
                                  (uint16_t)scratchpad[0U]);
    int16_t  raw_s  = (int16_t)raw_u;
    return (float)raw_s * 0.0625f;
}

/* ============================================================================
 *   PUBLIC API
 * ============================================================================ */

ErrorCode_t ds18b20_init(Ds18b20Handle_t       *handle,
                          const Ds18b20Config_t *config)
{
    if ((handle == NULL) || (config == NULL) || (config->port == NULL)) {
        return ERR_INVALID_PARAM;
    }

    handle->config       = *config;
    handle->initialized  = true;
    handle->last_temp_c  = DS18B20_INVALID_TEMP;
    handle->last_error   = SENSOR_OK;

    /* Release bus – idle HIGH via external pull-up */
    ow_release(&handle->config);

    return ERR_NONE;
}

SensorError_t ds18b20_start_conversion(Ds18b20Handle_t *handle)
{
    SensorError_t err;

    if ((handle == NULL) || (!handle->initialized)) {
        return SENSOR_NOT_FOUND;
    }

    err = ow_reset(&handle->config);
    if (err != SENSOR_OK) {
        handle->last_error = err;
        return err;
    }

    ow_write_byte(&handle->config, OW_CMD_SKIP_ROM);
    ow_write_byte(&handle->config, OW_CMD_CONVERT_T);

    /* Bus is now held HIGH by pull-up; DS18B20 pulls LOW while converting */
    handle->last_error = SENSOR_OK;
    return SENSOR_OK;
}

SensorError_t ds18b20_read(Ds18b20Handle_t *handle, float *temp_out)
{
    SensorError_t err;
    uint8_t       scratchpad[DS18B20_SCRATCHPAD_SIZE];

    if ((handle == NULL) || (temp_out == NULL) || (!handle->initialized)) {
        return SENSOR_NOT_FOUND;
    }

    err = ow_reset(&handle->config);
    if (err != SENSOR_OK) {
        handle->last_error = err;
        return err;
    }

    ow_write_byte(&handle->config, OW_CMD_SKIP_ROM);
    ow_write_byte(&handle->config, OW_CMD_READ_SCRATCHPAD);

    for (uint8_t i = 0U; i < DS18B20_SCRATCHPAD_SIZE; i++) {
        scratchpad[i] = ow_read_byte(&handle->config);
    }

    /* Byte [8] = CRC of bytes [0..7] */
    if (compute_crc8(scratchpad, 8U) != scratchpad[8U]) {
        handle->last_error = SENSOR_CRC_ERROR;
        return SENSOR_CRC_ERROR;
    }

    *temp_out           = convert_raw_to_celsius(scratchpad);
    handle->last_temp_c = *temp_out;
    handle->last_error  = SENSOR_OK;

    return SENSOR_OK;
}

SensorError_t ds18b20_read_blocking(Ds18b20Handle_t *handle, float *temp_out)
{
    SensorError_t err;

    if ((handle == NULL) || (temp_out == NULL)) {
        return SENSOR_NOT_FOUND;
    }

    err = ds18b20_start_conversion(handle);
    if (err != SENSOR_OK) {
        return err;
    }

    timing_delay_ms(DS18B20_CONVERSION_MS);

    return ds18b20_read(handle, temp_out);
}

SensorError_t ds18b20_get_last_error(const Ds18b20Handle_t *handle)
{
    if (handle == NULL) {
        return SENSOR_NOT_FOUND;
    }
    return handle->last_error;
}

float ds18b20_get_last_temp(const Ds18b20Handle_t *handle)
{
    if ((handle == NULL) || (!handle->initialized)) {
        return DS18B20_INVALID_TEMP;
    }
    return handle->last_temp_c;
}
