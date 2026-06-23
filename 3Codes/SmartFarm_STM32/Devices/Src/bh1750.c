/**
 * @file    bh1750.c
 * @brief   BH1750FVI driver implementation.
 * @author  Group SmartFarm
 * @date    2026-06-22
 */

#include "bh1750.h"
#include "timing.h" /* For timimg_delay_ms */

/* Private macros */
#define BH1750_CMD_POWER_DOWN  (0x00U)
#define BH1750_CMD_POWER_ON    (0x01U)
#define BH1750_CMD_RESET       (0x07U)

#define BH1750_MEASUREMENT_DELAY_MS_HIGH  (180U)  /* max 180ms for high-res */
#define BH1750_MEASUREMENT_DELAY_MS_LOW   (24U)   /* max 24ms for low-res */

/**
 * @brief   Send a single-byte command to the sensor.
 * @param   dev   Device context
 * @param   cmd   Command byte
 * @return  ErrorCode_t
 * @note    Uses HAL I2C transmit with timeout 100ms.
 */
static ErrorCode_t bh1750_write_command(const Bh1750_t *dev, uint8_t cmd)
{
    if (dev == NULL || dev->hi2c == NULL)
    {
        return ERR_INVALID_PARAM;
    }

    HAL_StatusTypeDef hal_status = HAL_I2C_Master_Transmit(dev->hi2c,
                                                           (uint16_t)(dev->addr << 1U),
                                                           &cmd,
                                                           1U,
                                                           HAL_MAX_DELAY); /* or 100ms */
    /* Use HAL_MAX_DELAY to wait indefinitely, or define a timeout */
    if (hal_status == HAL_OK)
    {
        return ERR_NONE;
    }
    else if (hal_status == HAL_ERROR)
    {
        return ERR_I2C_TX_FAIL;
    }
    else if (hal_status == HAL_BUSY)
    {
        return ERR_I2C_TX_FAIL;  /* or ERR_BUSY */
    }
    else if (hal_status == HAL_TIMEOUT)
    {
        return ERR_I2C_TX_FAIL;
    }
    else
    {
        return ERR_I2C_TX_FAIL;
    }
}

/**
 * @brief   Read two data bytes from the sensor.
 * @param   dev   Device context
 * @param   buf   Buffer to store two bytes (MSB first)
 * @return  ErrorCode_t
 */
static ErrorCode_t bh1750_read_data(const Bh1750_t *dev, uint8_t buf[2])
{
    if (dev == NULL || dev->hi2c == NULL || buf == NULL)
    {
        return ERR_INVALID_PARAM;
    }

    HAL_StatusTypeDef hal_status = HAL_I2C_Master_Receive(dev->hi2c,
                                                          (uint16_t)(dev->addr << 1U),
                                                          buf,
                                                          2U,
                                                          HAL_MAX_DELAY);
    if (hal_status == HAL_OK)
    {
        return ERR_NONE;
    }
    else if (hal_status == HAL_ERROR)
    {
        return ERR_I2C_RX_FAIL;
    }
    else if (hal_status == HAL_BUSY)
    {
        return ERR_I2C_RX_FAIL;
    }
    else if (hal_status == HAL_TIMEOUT)
    {
        return ERR_I2C_RX_FAIL;
    }
    else
    {
        return ERR_I2C_RX_FAIL;
    }
}

/**
 * @brief   Compute lux from raw reading based on mode.
 * @param   raw  16-bit raw value (MSB first combined)
 * @param   mode Current measurement mode
 * @return  Illuminance in lux as float.
 */
static float bh1750_convert_to_lux(uint16_t raw, Bh1750Mode_t mode)
{
    float lux;

    /* Basic formula: lux = raw / 1.2 */
    const float measurement_accuracy = 1.2f;

    if (mode == BH1750_MODE_CONT_HIGH_RES2 || mode == BH1750_MODE_ONE_TIME_HIGH_RES2)
    {
        /* For H-Resolution Mode2, resolution is 0.5 lx, i.e., divide by 2.4 */
        lux = (float)raw / (measurement_accuracy * 2.0f);
    }
    else
    {
        /* For H-Resolution and L-Resolution modes, divide by 1.2 */
        lux = (float)raw / measurement_accuracy;
    }

    return lux;
}

/* ------------------------------------------------------------------------- */
/* Public API implementation                                                 */
/* ------------------------------------------------------------------------- */

ErrorCode_t bh1750_init(Bh1750_t *dev,
                        I2C_HandleTypeDef *hi2c,
                        uint8_t addr,
                        Bh1750Mode_t mode)
{
    ErrorCode_t err;

    if (dev == NULL || hi2c == NULL)
    {
        return ERR_INVALID_PARAM;
    }

    /* Store context */
    dev->hi2c = hi2c;
    dev->addr = addr;
    dev->mode = mode;
    dev->mtreg = 69U;  /* default */

    /* Power on */
    err = bh1750_power_on(dev);
    if (err != ERR_NONE)
    {
        return err;
    }

    /* Small delay after power on (datasheet recommends) */
    timing_delay_ms(10U);

    /* Set measurement mode */
    err = bh1750_set_mode(dev, mode);
    if (err != ERR_NONE)
    {
        return err;
    }

    return ERR_NONE;
}

ErrorCode_t bh1750_read_lux(Bh1750_t *dev, float *lux)
{
    uint8_t raw_bytes[2];
    uint16_t raw;
    ErrorCode_t err;

    if (dev == NULL || lux == NULL)
    {
        return ERR_INVALID_PARAM;
    }

    /* Read two bytes from sensor */
    err = bh1750_read_data(dev, raw_bytes);
    if (err != ERR_NONE)
    {
        return err;
    }

    /* Combine MSB and LSB (MSB first) */
    raw = (uint16_t)(((uint16_t)raw_bytes[0] << 8U) | (uint16_t)raw_bytes[1]);

    /* Convert to lux based on mode */
    *lux = bh1750_convert_to_lux(raw, dev->mode);

    return ERR_NONE;
}

ErrorCode_t bh1750_set_mode(Bh1750_t *dev, Bh1750Mode_t mode)
{
    ErrorCode_t err;
    uint32_t delay_ms;

    if (dev == NULL)
    {
        return ERR_INVALID_PARAM;
    }

    /* Send the mode command */
    err = bh1750_write_command(dev, (uint8_t)mode);
    if (err != ERR_NONE)
    {
        return err;
    }

    /* Wait for measurement completion based on mode */
    if ((mode == BH1750_MODE_CONT_HIGH_RES) ||
        (mode == BH1750_MODE_CONT_HIGH_RES2) ||
        (mode == BH1750_MODE_ONE_TIME_HIGH_RES) ||
        (mode == BH1750_MODE_ONE_TIME_HIGH_RES2))
    {
        delay_ms = BH1750_MEASUREMENT_DELAY_MS_HIGH;
    }
    else
    {
        /* Low resolution modes */
        delay_ms = BH1750_MEASUREMENT_DELAY_MS_LOW;
    }

    timing_delay_ms(delay_ms);

    /* Update context mode */
    dev->mode = mode;

    return ERR_NONE;
}

ErrorCode_t bh1750_power_down(Bh1750_t *dev)
{
    if (dev == NULL)
    {
        return ERR_INVALID_PARAM;
    }
    return bh1750_write_command(dev, BH1750_CMD_POWER_DOWN);
}

ErrorCode_t bh1750_power_on(Bh1750_t *dev)
{
    if (dev == NULL)
    {
        return ERR_INVALID_PARAM;
    }
    return bh1750_write_command(dev, BH1750_CMD_POWER_ON);
}
