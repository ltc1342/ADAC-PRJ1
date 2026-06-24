/**
 * @file    soil_moisture.c
 * @brief   Soil moisture sensor implementation (SMS-V1 LM393) – ADC1/DMA.
 * @author  ltc1342
 * @date    2026-07-01
 *
 * @note    DMA buffer layout:
 *            dma_buf[0..SOIL_OVERSAMPLE_COUNT-1] receive SOIL_OVERSAMPLE_COUNT
 *            consecutive ADC1 conversions triggered by a single
 *            HAL_ADC_Start_DMA() call (scan mode OFF, single channel,
 *            nbrOfConversion = SOIL_OVERSAMPLE_COUNT set in CubeMX).
 *
 *          Power-gate timing (when pwr_pin.port != NULL):
 *            soil_moisture_start()  : GPIO HIGH (VCC on), record timestamp
 *            soil_moisture_update() : after SOIL_POWERUP_DELAY_MS → ADC start
 *            soil_moisture_update() : after DMA done → GPIO LOW (VCC off)
 */

#include "soil_moisture.h"
#include "timing.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_ll_gpio.h"

/* ============================================================================
 *   PRIVATE CONSTANTS
 * ============================================================================ */

/** @brief Stabilisation delay after asserting sensor VCC [ms]. */
#define SOIL_POWERUP_DELAY_MS   20U

/* ============================================================================
 *   PRIVATE HELPERS – power gate
 * ============================================================================ */

/**
 * @brief  Return true when power-gating is configured.
 */
static bool has_power_gate(const SoilMoistureHandle_t *handle)
{
    return (handle->pwr_pin.port != NULL);
}

/**
 * @brief  Assert sensor VCC (HIGH = VCC on, assuming p-channel or active-high driver).
 * @note   Adjust polarity here if your gate driver is active-LOW.
 */
static void pwr_on(const SoilMoistureHandle_t *handle)
{
    if (has_power_gate(handle)) {
        LL_GPIO_SetOutputPin(handle->pwr_pin.port, handle->pwr_pin.pin);
    }
}

/**
 * @brief  De-assert sensor VCC (LOW = VCC off).
 */
static void pwr_off(const SoilMoistureHandle_t *handle)
{
    if (has_power_gate(handle)) {
        LL_GPIO_ResetOutputPin(handle->pwr_pin.port, handle->pwr_pin.pin);
    }
}

/* ============================================================================
 *   PRIVATE HELPER – raw → percentage
 * ============================================================================ */

/**
 * @brief  Compute arithmetic mean of the DMA oversampling buffer.
 * @return Average raw ADC value (0–4095).
 */
static uint16_t compute_average_raw(const volatile uint16_t *buf, uint8_t count)
{
    uint32_t sum = 0U;

    for (uint8_t i = 0U; i < count; i++) {
        sum += (uint32_t)buf[i];
    }

    return (uint16_t)(sum / (uint32_t)count);
}

/**
 * @brief  Convert a 12-bit raw ADC value to soil moisture percentage.
 *
 * @details  Linear interpolation between calibration endpoints:
 *             moisture = (DRY - raw) / (DRY - WET) × 100
 *           Clamped to [0, 100] %.
 *
 * @param  raw  12-bit ADC reading (0–4095).
 * @return Moisture percentage (0.0–100.0 %).
 */
static float raw_to_percent(uint16_t raw)
{
    /* Guard against inverted calibration to prevent division by zero */
    if (SOIL_ADC_DRY <= SOIL_ADC_WET) {
        return 0.0f;
    }

    if (raw >= SOIL_ADC_DRY) {
        return 0.0f;
    }

    if (raw <= SOIL_ADC_WET) {
        return 100.0f;
    }

    float numerator   = (float)(SOIL_ADC_DRY - raw);
    float denominator = (float)(SOIL_ADC_DRY - SOIL_ADC_WET);

    return (numerator / denominator) * 100.0f;
}

/* ============================================================================
 *   PRIVATE HELPER – start ADC DMA
 * ============================================================================ */

/**
 * @brief  Arm ADC1 DMA for SOIL_OVERSAMPLE_COUNT samples.
 * @return ERR_NONE on success, ERR_ADC_CONV_FAIL on HAL error.
 */
static ErrorCode_t start_adc_dma(SoilMoistureHandle_t *handle)
{
    HAL_StatusTypeDef status;

    /* Cast away volatile for HAL – DMA writes to this buffer from hardware */
    status = HAL_ADC_Start_DMA(handle->hadc,
                                (uint32_t *)(uintptr_t)handle->dma_buf,
                                (uint32_t)SOIL_OVERSAMPLE_COUNT);

    if (status != HAL_OK) {
        handle->state      = SOIL_STATE_ERROR;
        handle->last_error = SENSOR_TIMEOUT;
        return ERR_ADC_CONV_FAIL;
    }

    handle->state       = SOIL_STATE_CONVERTING;
    handle->last_error  = SENSOR_OK;
    return ERR_NONE;
}

/* ============================================================================
 *   PUBLIC API
 * ============================================================================ */

ErrorCode_t soil_moisture_init(SoilMoistureHandle_t        *handle,
                                ADC_HandleTypeDef            *hadc,
                                const SoilMoisturePwrPin_t  *pwr_pin)
{
    if ((handle == NULL) || (hadc == NULL)) {
        return ERR_INVALID_PARAM;
    }

    handle->hadc               = hadc;
    handle->dma_complete       = false;
    handle->state              = SOIL_STATE_IDLE;
    handle->powerup_start_ms   = 0U;
    handle->last_percent       = 0.0f;
    handle->last_error         = SENSOR_OK;
    handle->initialized        = true;

    if (pwr_pin != NULL) {
        handle->pwr_pin = *pwr_pin;
    } else {
        handle->pwr_pin.port = NULL;
        handle->pwr_pin.pin  = 0U;
    }

    /* Ensure sensor VCC is OFF at init */
    pwr_off(handle);

    return ERR_NONE;
}

ErrorCode_t soil_moisture_start(SoilMoistureHandle_t *handle)
{
    if ((handle == NULL) || (!handle->initialized)) {
        return ERR_INVALID_PARAM;
    }

    /* Reject if a conversion is already in flight */
    if ((handle->state == SOIL_STATE_CONVERTING) ||
        (handle->state == SOIL_STATE_POWERING_UP)) {
        return ERR_NONE;   /* silently ignore – already running */
    }

    handle->dma_complete = false;

    if (has_power_gate(handle)) {
        pwr_on(handle);
        handle->powerup_start_ms = timing_get_ms();
        handle->state            = SOIL_STATE_POWERING_UP;
    } else {
        /* No power-gate → start ADC immediately */
        return (start_adc_dma(handle) == ERR_NONE)
                   ? ERR_NONE
                   : ERR_ADC_CONV_FAIL;
    }

    return ERR_NONE;
}

void soil_moisture_update(SoilMoistureHandle_t *handle)
{
    if ((handle == NULL) || (!handle->initialized)) {
        return;
    }

    switch (handle->state) {

        case SOIL_STATE_POWERING_UP: {
            uint32_t elapsed = timing_get_ms() - handle->powerup_start_ms;
            if (elapsed >= SOIL_POWERUP_DELAY_MS) {
                /* VCC stabilised – arm the DMA */
                if (start_adc_dma(handle) != ERR_NONE) {
                    pwr_off(handle);   /* clean up on failure */
                }
            }
            break;
        }

        case SOIL_STATE_CONVERTING: {
            /* Check DMA timeout */
            uint32_t elapsed = timing_get_ms() - handle->powerup_start_ms;
            uint32_t total_timeout = SOIL_POWERUP_DELAY_MS + SOIL_DMA_TIMEOUT_MS;

            if (handle->dma_complete) {
                /* DMA finished – compute result */
                HAL_ADC_Stop_DMA(handle->hadc);
                pwr_off(handle);

                uint16_t raw        = compute_average_raw(handle->dma_buf,
                                                          SOIL_OVERSAMPLE_COUNT);
                handle->last_percent = raw_to_percent(raw);
                handle->last_error   = SENSOR_OK;
                handle->state        = SOIL_STATE_DATA_READY;

            } else if (elapsed > total_timeout) {
                /* Timeout: abort and clean up */
                HAL_ADC_Stop_DMA(handle->hadc);
                pwr_off(handle);
                handle->last_error = SENSOR_TIMEOUT;
                handle->state      = SOIL_STATE_ERROR;
            }
            break;
        }

        case SOIL_STATE_IDLE:       /* fall-through – nothing to do */
        case SOIL_STATE_DATA_READY:
        case SOIL_STATE_ERROR:
        default:
            break;
    }
}

void soil_moisture_dma_callback(SoilMoistureHandle_t *handle)
{
    /* ISR context: set flag only, no processing */
    if (handle != NULL) {
        handle->dma_complete = true;
    }
}

SensorError_t soil_moisture_get_percent(SoilMoistureHandle_t *handle,
                                         float                *percent_out)
{
    if ((handle == NULL) || (percent_out == NULL) || (!handle->initialized)) {
        return SENSOR_NOT_FOUND;
    }

    switch (handle->state) {

        case SOIL_STATE_DATA_READY:
            *percent_out       = handle->last_percent;
            handle->state      = SOIL_STATE_IDLE;   /* consume the data */
            return SENSOR_OK;

        case SOIL_STATE_ERROR:
            handle->state = SOIL_STATE_IDLE;         /* allow retry */
            return handle->last_error;

        case SOIL_STATE_IDLE:
        case SOIL_STATE_POWERING_UP:
        case SOIL_STATE_CONVERTING:
        default:
            return SENSOR_BUSY;
    }
}

SensorError_t soil_moisture_read_blocking(SoilMoistureHandle_t *handle,
                                           float                *percent_out)
{
    SensorError_t  err;
    ErrorCode_t    start_err;
    uint32_t       deadline_ms;

    if ((handle == NULL) || (percent_out == NULL)) {
        return SENSOR_NOT_FOUND;
    }

    start_err = soil_moisture_start(handle);
    if (start_err != ERR_NONE) {
        return SENSOR_TIMEOUT;
    }

    deadline_ms = timing_get_ms() +
                  SOIL_POWERUP_DELAY_MS +
                  SOIL_DMA_TIMEOUT_MS +
                  10U;   /* small margin */

    /* Poll FSM until done or timeout */
    while ((handle->state != SOIL_STATE_DATA_READY) &&
           (handle->state != SOIL_STATE_ERROR)) {
        soil_moisture_update(handle);
        timing_delay_ms(10U);

        if (timing_get_ms() >= deadline_ms) {
            HAL_ADC_Stop_DMA(handle->hadc);
            pwr_off(handle);
            handle->state      = SOIL_STATE_ERROR;
            handle->last_error = SENSOR_TIMEOUT;
            return SENSOR_TIMEOUT;
        }
    }

    err = soil_moisture_get_percent(handle, percent_out);
    return err;
}

SensorError_t soil_moisture_get_raw(const SoilMoistureHandle_t *handle,
                                     uint16_t                   *raw_out)
{
    if ((handle == NULL) || (raw_out == NULL) || (!handle->initialized)) {
        return SENSOR_NOT_FOUND;
    }

    if (handle->state == SOIL_STATE_DATA_READY) {
        *raw_out = compute_average_raw(handle->dma_buf, SOIL_OVERSAMPLE_COUNT);
        return SENSOR_OK;
    }

    if ((handle->state == SOIL_STATE_CONVERTING) ||
        (handle->state == SOIL_STATE_POWERING_UP)) {
        return SENSOR_BUSY;
    }

    return handle->last_error;
}

SensorError_t soil_moisture_get_last_error(const SoilMoistureHandle_t *handle)
{
    if (handle == NULL) {
        return SENSOR_NOT_FOUND;
    }
    return handle->last_error;
}

SoilMoistureState_t soil_moisture_get_state(const SoilMoistureHandle_t *handle)
{
    if (handle == NULL) {
        return SOIL_STATE_ERROR;
    }
    return handle->state;
}
