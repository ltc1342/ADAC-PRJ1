/**
 * @file    bh1750.h
 * @brief   bh1750FVI ambient light sensor driver.
 * @author  Group SmartFarm
 * @date    2026-06-22
 *
 * @note    Supports multiple measurement modes and uses common error codes.
 */

#ifndef bh1750_H
#define bh1750_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "i2c.h"
#include "app_types.h"   /* ErrorCode_t */

/* ============================================================================
 *   CONSTANTS
 * ============================================================================ */

/** @brief Default I2C slave address (ADDR = L). */
#define BH1750_I2C_ADDR_DEFAULT   (0x23U)

/** @brief Alternative I2C slave address (ADDR = H). */
#define BH1750_I2C_ADDR_ALT       (0x5CU)

/* ============================================================================
 *   ENUMERATIONS
 * ============================================================================ */

/** @brief Measurement modes supported by bh1750. */
typedef enum {
    BH1750_MODE_CONT_HIGH_RES    = 0x10U,   /**< Continuous, 1 lx res, 120ms */
    BH1750_MODE_CONT_HIGH_RES2   = 0x11U,   /**< Continuous, 0.5 lx res, 120ms */
    BH1750_MODE_CONT_LOW_RES     = 0x13U,   /**< Continuous, 4 lx res, 16ms */
    BH1750_MODE_ONE_TIME_HIGH_RES = 0x20U,  /**< One-shot, 1 lx res, 120ms, then power down */
    BH1750_MODE_ONE_TIME_HIGH_RES2= 0x21U,  /**< One-shot, 0.5 lx res, 120ms, then power down */
    BH1750_MODE_ONE_TIME_LOW_RES  = 0x23U   /**< One-shot, 4 lx res, 16ms, then power down */
} Bh1750Mode_t;

/* ============================================================================
 *   STRUCTURES
 * ============================================================================ */

/**
 * @brief bh1750 device context.
 * @note  Stores I2C handle, slave address, and current measurement mode.
 */
typedef struct {
    I2C_HandleTypeDef *hi2c;    /**< Pointer to HAL I2C handle */
    uint8_t addr;               /**< I2C slave address (7-bit) */
    Bh1750Mode_t mode;          /**< Currently active measurement mode */
    uint16_t mtreg;             /**< Measurement time register value (default 69) */
} Bh1750_t;

/* ============================================================================
 *   FUNCTION PROTOTYPES
 * ============================================================================ */

/**
 * @brief   Initialize bh1750 sensor.
 * @param   dev   Pointer to device context (must be allocated by caller)
 * @param   hi2c  Pointer to initialized I2C handle
 * @param   addr  I2C slave address (e.g. bh1750_I2C_ADDR_DEFAULT)
 * @param   mode  Desired measurement mode
 * @return  ErrorCode_t: ERR_NONE on success, otherwise appropriate error.
 * @note    This function sends Power On, waits, then sends the mode command.
 *          After this, the sensor is ready to measure.
 */
ErrorCode_t bh1750_init(Bh1750_t *dev,
                        I2C_HandleTypeDef *hi2c,
                        uint8_t addr,
                        Bh1750Mode_t mode);

/**
 * @brief   Read illuminance in lux.
 * @param   dev  Pointer to device context (must be initialized)
 * @param   lux  Pointer to float to store the result (lux)
 * @return  ErrorCode_t: ERR_NONE on success, otherwise appropriate error.
 * @note    The measurement result is read as two bytes and converted to lux.
 *          The calculation depends on the mode set during initialization.
 */
ErrorCode_t bh1750_read_lux(Bh1750_t *dev, float *lux);

/**
 * @brief   Change measurement mode at runtime.
 * @param   dev   Pointer to device context
 * @param   mode  New measurement mode
 * @return  ErrorCode_t: ERR_NONE on success.
 * @note    This sends the new mode command and waits for the measurement duration.
 *          It does not power down; if one-shot mode is used, it will power down after measurement.
 */
ErrorCode_t bh1750_set_mode(Bh1750_t *dev, Bh1750Mode_t mode);

/**
 * @brief   Power down the sensor.
 * @param   dev  Pointer to device context
 * @return  ErrorCode_t: ERR_NONE on success.
 * @note    Sends the Power Down command (0x00). The device enters low-power state.
 */
ErrorCode_t bh1750_power_down(Bh1750_t *dev);

/**
 * @brief   Power on the sensor.
 * @param   dev  Pointer to device context
 * @return  ErrorCode_t: ERR_NONE on success.
 * @note    Sends the Power On command (0x01). Device waits for measurement command.
 */
ErrorCode_t bh1750_power_on(Bh1750_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* bh1750_H */
