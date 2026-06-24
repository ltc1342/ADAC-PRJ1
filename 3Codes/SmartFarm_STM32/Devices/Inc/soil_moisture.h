/**
 * @file    soil_moisture.h
 * @brief   Soil moisture sensor driver (SMS-V1 LM393) – ADC1 CH1 with DMA.
 * @author  ltc1342
 * @date    2026-07-01
 *
 * @note    Hardware connection:
 *            Sensor AO pin → PA1 (ADC1_IN1)
 *            Sensor VCC    → controlled via a GPIO (power-gating to prevent
 *                            probe corrosion; see SOIL_PWR_PORT / SOIL_PWR_PIN)
 *            Sensor GND    → GND
 *
 *          ADC configuration (set in CubeMX, do NOT re-configure here):
 *            - ADC1, Channel 1 (IN1), 12-bit resolution
 *            - Continuous conversion mode: DISABLED
 *            - DMA: ADC1 → DMA2 Stream0 Channel0, circular, half-word
 *            - Scan mode: DISABLED (single channel)
 *            - Trigger: software start
 *            - Sample time: 480 cycles (≈ 5 µs @ 96 MHz APB2/2 = 48 MHz ADC)
 *
 *          Voltage-to-moisture mapping (LM393, 3.3V supply):
 *            Dry   → ADC ≈ 4095 (3.3V)   → 0 % moisture
 *            Wet   → ADC ≈  820 (0.66V)  → 100 % moisture
 *            SOIL_ADC_DRY  / SOIL_ADC_WET are calibration macros in app_defs.h;
 *            override them to match your specific probe + soil type.
 *
 *          DMA completion callback – wire CubeMX-generated callback:
 *          @code
 *            void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
 *                if (hadc->Instance == ADC1) {
 *                    soil_moisture_dma_callback(&g_soil);
 *                }
 *            }
 *          @endcode
 *
 *          Non-blocking usage (recommended):
 *          @code
 *            extern ADC_HandleTypeDef hadc1;
 *            static SoilMoistureHandle_t g_soil;
 *
 *            SoilMoisturePwrPin_t pwr = { GPIOB, LL_GPIO_PIN_10 };  // example
 *            soil_moisture_init(&g_soil, &hadc1, &pwr);
 *
 *            // Trigger once every SENSOR_READ_INTERVAL_MS:
 *            soil_moisture_start(&g_soil);
 *            // ...DMA fires HAL_ADC_ConvCpltCallback → soil_moisture_dma_callback()
 *
 *            float pct;
 *            if (soil_moisture_get_percent(&g_soil, &pct) == SENSOR_OK) { ... }
 *          @endcode
 *
 *          Blocking usage (simpler, suitable for Level-1 tasks):
 *          @code
 *            float pct;
 *            soil_moisture_read_blocking(&g_soil, &pct);
 *          @endcode
 */

#ifndef SOIL_MOISTURE_H_
#define SOIL_MOISTURE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"
#include "stm32f4xx_ll_gpio.h"
#include "app_types.h"

/* ============================================================================
 *   CALIBRATION DEFAULTS  (override in app_defs.h if needed)
 * ============================================================================ */

#ifndef SOIL_ADC_DRY
/** @brief 12-bit ADC raw reading when probe is in dry air. */
#define SOIL_ADC_DRY   3900U
#endif

#ifndef SOIL_ADC_WET
/** @brief 12-bit ADC raw reading when probe is fully submerged in water. */
#define SOIL_ADC_WET    820U
#endif

/** @brief Number of DMA samples averaged per reading (power of 2, ≥ 1). */
#define SOIL_OVERSAMPLE_COUNT   8U

/** @brief Timeout waiting for DMA completion in blocking mode [ms]. */
#define SOIL_DMA_TIMEOUT_MS   100U

/* ============================================================================
 *   TYPES
 * ============================================================================ */

/**
 * @brief  Optional GPIO used to power-gate the sensor VCC.
 * @note   Set port to NULL to disable power-gating (VCC always on).
 *         When power-gating is used, the driver asserts VCC for
 *         SOIL_POWERUP_DELAY_MS before triggering the ADC, then de-asserts
 *         after the conversion completes.
 */
typedef struct {
    GPIO_TypeDef *port;   /**< GPIO port for VCC switch (NULL = no gating) */
    uint32_t      pin;    /**< LL pin mask                                  */
} SoilMoisturePwrPin_t;

/**
 * @brief  Driver state machine states.
 */
typedef enum {
    SOIL_STATE_IDLE        = 0U,   /**< Ready to accept a new measurement   */
    SOIL_STATE_POWERING_UP = 1U,   /**< Waiting for VCC to stabilise        */
    SOIL_STATE_CONVERTING  = 2U,   /**< DMA conversion in progress          */
    SOIL_STATE_DATA_READY  = 3U,   /**< Raw data available, not yet read    */
    SOIL_STATE_ERROR       = 4U    /**< Last operation failed               */
} SoilMoistureState_t;

/**
 * @brief  Soil moisture driver handle.
 * @note   Caller allocates storage (static recommended).
 *         dma_buf is the landing buffer for DMA – must remain valid for the
 *         entire lifetime of the handle (do not place on the stack of a
 *         short-lived function).
 */
typedef struct {
    ADC_HandleTypeDef    *hadc;                          /**< CubeMX ADC handle           */
    SoilMoisturePwrPin_t  pwr_pin;                       /**< VCC power-gate config       */
    volatile uint16_t     dma_buf[SOIL_OVERSAMPLE_COUNT];/**< DMA destination buffer      */
    volatile bool         dma_complete;                  /**< Set by ISR callback         */
    SoilMoistureState_t   state;                         /**< Internal FSM state          */
    uint32_t              powerup_start_ms;              /**< timing_get_ms() at VCC on   */
    float                 last_percent;                  /**< Most recent valid reading   */
    SensorError_t         last_error;                    /**< Result of last operation    */
    bool                  initialized;
} SoilMoistureHandle_t;

/* ============================================================================
 *   API
 * ============================================================================ */

/**
 * @brief  Initialise soil moisture driver.
 * @param  handle   Driver handle (must not be NULL).
 * @param  hadc     CubeMX-generated ADC handle (ADC1, must not be NULL).
 * @param  pwr_pin  Optional VCC power-gate GPIO (pass NULL port to disable).
 * @return ERR_NONE on success, ERR_INVALID_PARAM on NULL handle or hadc.
 *
 * @note   Does NOT start a conversion. Call soil_moisture_start() after init.
 */
ErrorCode_t soil_moisture_init(SoilMoistureHandle_t        *handle,
                                ADC_HandleTypeDef            *hadc,
                                const SoilMoisturePwrPin_t  *pwr_pin);

/**
 * @brief  Trigger a non-blocking DMA conversion.
 * @param  handle  Initialised driver handle.
 * @return ERR_NONE        – conversion started (or VCC power-up started).
 *         ERR_ADC_CONV_FAIL – HAL_ADC_Start_DMA failed.
 *         ERR_INVALID_PARAM – NULL handle or not initialised.
 *
 * @note   If power-gating is enabled, VCC is asserted here; the actual
 *         ADC start is deferred until soil_moisture_update() detects that
 *         the stabilisation delay has elapsed.
 *         Call soil_moisture_update() periodically (e.g. every 10 ms) and
 *         read the result via soil_moisture_get_percent() once it returns
 *         SENSOR_OK.
 */
ErrorCode_t soil_moisture_start(SoilMoistureHandle_t *handle);

/**
 * @brief  Drive the internal FSM – call from the main loop (every ~10 ms).
 * @param  handle  Initialised driver handle.
 *
 * @note   Handles:
 *           - Power-up delay expiry  → starts ADC DMA
 *           - DMA complete flag      → computes average, de-asserts VCC,
 *                                      transitions to DATA_READY
 *           - Timeout detection      → transitions to ERROR
 */
void soil_moisture_update(SoilMoistureHandle_t *handle);

/**
 * @brief  ISR-safe callback – call from HAL_ADC_ConvCpltCallback().
 * @param  handle  Driver handle (the ADC1 instance).
 *
 * @note   Sets the dma_complete flag; all processing happens in
 *         soil_moisture_update() (task context, not ISR).
 */
void soil_moisture_dma_callback(SoilMoistureHandle_t *handle);

/**
 * @brief  Read the latest converted moisture percentage.
 * @param  handle      Initialised driver handle.
 * @param  percent_out Destination for moisture value 0.0–100.0 % (must not be NULL).
 * @return SENSOR_OK        – fresh value written to *percent_out; state → IDLE.
 *         SENSOR_BUSY      – conversion still in progress.
 *         SENSOR_TIMEOUT   – DMA did not complete within SOIL_DMA_TIMEOUT_MS.
 *         SENSOR_NOT_FOUND – handle NULL or not initialised.
 *
 * @note   After reading SENSOR_OK the internal state returns to IDLE; call
 *         soil_moisture_start() again for the next measurement.
 */
SensorError_t soil_moisture_get_percent(SoilMoistureHandle_t *handle,
                                         float                *percent_out);

/**
 * @brief  Blocking convenience wrapper: power on → convert → read → power off.
 * @param  handle      Initialised driver handle.
 * @param  percent_out Destination for moisture value 0.0–100.0 %.
 * @return SENSOR_OK on success, SENSOR_TIMEOUT on DMA timeout.
 *
 * @note   Polls soil_moisture_update() with timing_delay_ms(10) until done
 *         or SOIL_DMA_TIMEOUT_MS expires. Suitable for Level-1 bare-metal
 *         super-loop; avoid in latency-sensitive applications.
 */
SensorError_t soil_moisture_read_blocking(SoilMoistureHandle_t *handle,
                                           float                *percent_out);

/**
 * @brief  Return the raw 12-bit ADC average of the last completed conversion.
 * @param  handle   Initialised driver handle.
 * @param  raw_out  Destination for raw value (0–4095).
 * @return SENSOR_OK or SENSOR_BUSY / SENSOR_NOT_FOUND.
 */
SensorError_t soil_moisture_get_raw(const SoilMoistureHandle_t *handle,
                                     uint16_t                   *raw_out);

/**
 * @brief  Return the cached error from the last operation.
 * @param  handle  Initialised driver handle.
 * @return Last SensorError_t, or SENSOR_NOT_FOUND if handle is NULL.
 */
SensorError_t soil_moisture_get_last_error(const SoilMoistureHandle_t *handle);

/**
 * @brief  Return the current FSM state (for diagnostics / display).
 * @param  handle  Initialised driver handle.
 * @return Current SoilMoistureState_t, or SOIL_STATE_ERROR on NULL.
 */
SoilMoistureState_t soil_moisture_get_state(const SoilMoistureHandle_t *handle);

#ifdef __cplusplus
}
#endif

#endif /* SOIL_MOISTURE_H_ */
