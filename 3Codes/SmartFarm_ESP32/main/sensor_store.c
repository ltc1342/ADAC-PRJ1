/**
 * @file sensor_store.c
 * @brief Mutex-protected sensor data store implementation.
 *
 * Implementation notes:
 *  - Uses a statically-allocated FreeRTOS mutex (no heap fragmentation).
 *  - Both set and get use a 100 ms timeout; a missed window is logged
 *    but not treated as fatal — the next UART frame will overwrite.
 *  - memcpy is used instead of struct assignment to guarantee atomicity
 *    on the field level even for compilers that may split wide writes.
 */

#include "sensor_store.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "SENSOR_STORE";

/* Statically-allocated mutex handle and data buffer */
static StaticSemaphore_t  s_mutex_buf;
static SemaphoreHandle_t  s_mutex = NULL;
static sensor_data_t      s_data  = { .valid = false };

/* ── Public API ──────────────────────────────────────────────────────────── */

esp_err_t sensor_store_init(void)
{
    s_mutex = xSemaphoreCreateMutexStatic(&s_mutex_buf);
    if (s_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }
    memset(&s_data, 0, sizeof(s_data));
    ESP_LOGI(TAG, "Sensor store ready");
    return ESP_OK;
}

void sensor_store_set(const sensor_data_t *data)
{
    if (data == NULL) return;

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        memcpy(&s_data, data, sizeof(sensor_data_t));
        xSemaphoreGive(s_mutex);
    } else {
        ESP_LOGW(TAG, "sensor_store_set: mutex timeout");
    }
}

void sensor_store_get(sensor_data_t *out)
{
    if (out == NULL) return;

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        memcpy(out, &s_data, sizeof(sensor_data_t));
        xSemaphoreGive(s_mutex);
    } else {
        ESP_LOGW(TAG, "sensor_store_get: mutex timeout");
    }
}
