/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "i2c.h"
#include "rtc.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdint.h>
#include <stdio.h>

/* Common */
#include "timing.h"
#include "debug_log.h"
#include "app_config.h"
#include "app_defs.h"

/* Device drivers */
#include "dht.h"
#include "bh1750.h"
#include "ds18b20.h"
#include "relay.h"
#include "sh1106.h"          /* For display driver functions */

/* Services */
#include "sensor_manager.h"
#include "relay_manager.h"
#include "rtc_manager.h"
#include "schedule_manager.h"

/* App */
#include "control_manager.h"
#include "display_manager.h"
#include "communication_manager.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* ============================================================================
 *   PRIVATE MACROS
 * ============================================================================ */

/** Main loop delay (ms) – prevents CPU hogging */
#define MAIN_LOOP_DELAY_MS     100U

/** Sensor read interval (must match SENSOR_READ_INTERVAL_MS from app_defs.h) */
#define SENSOR_READ_PERIOD_MS  SENSOR_READ_INTERVAL_MS
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* ============================================================================
 *   PRIVATE VARIABLES
 * ============================================================================ */

/* ----- Device handles (static) ----- */
static DhtHandle_t      g_dht_handle;
static Bh1750_t         g_bh1750_dev;
static Ds18b20Handle_t  g_ds18b20_handle;
static RelayHandle_t    g_relay_handle;

/* ----- Service instances (static) ----- */
static SensorManager_t      g_sensor_mgr;
static RelayManager_t       g_relay_mgr;
static RtcManager_t         g_rtc_mgr;
static ScheduleManager_t    g_schedule_mgr;

/* ----- App instances (static) ----- */
static ControlManager_t     g_control_mgr;
static DisplayManager_t     g_display_mgr;
static CommManager_t        g_comm_mgr;

/* ----- Display driver function table (for display_manager) ----- */
static DisplayDriverFns_t   g_display_driver;

/* ----- Status snapshots ----- */
static RelayStatus_t        g_relay_status;
static TimeOfDay_t          g_current_time;
static DateOfDay_t          g_last_date;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* ============================================================================
 *   ADAPTER FUNCTIONS (for SensorManager)
 * ============================================================================ */

/**
 * @brief  Adapter for DHT11 – reads temperature and humidity.
 */
static SensorError_t dht11_adapter_read(void *handle, float *temp_out, float *hum_out);

/**
 * @brief  Adapter for BH1750 – reads illuminance.
 */
static SensorError_t bh1750_adapter_read(void *handle, float *lux_out);

/**
 * @brief  Adapter for DS18B20 – reads soil temperature.
 */
static SensorError_t ds18b20_adapter_read(void *handle, float *temp_out);

/**
 * @brief  Adapter for soil moisture (ADC) – reads LM393 sensor.
 */
static SensorError_t soil_adapter_read(void *handle, float *moisture_out);
/* ============================================================================
 *   COMMUNICATION CALLBACK (from ESP32)
 * ============================================================================ */

/**
 * @brief  Callback invoked by communication_manager when a valid command
 *         is received from the ESP32.
 */
static void on_command_received(const ParsedCommand_t *cmd, void *user_data);
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_RTC_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  /* ---- Custom initialisation ---- */
      timing_init();          /* DWT cycle counter */
      debug_log_init();       /* SWO or UART debug output */

      debug_log("\n=== SmartFarm STM32 Booting ===\n");

      /* ---- Device driver initialisation ---- */
      /* DHT11 */
      DhtConfig_t dht_cfg = {
          .data = { .port = DHT11_GPIO_Port, .pin = DHT11_Pin },
          .type = DHT_TYPE_DHT11
      };
      if (dht_init(&g_dht_handle, &dht_cfg) != DHT_OK)
      {
          debug_log("DHT11 init failed\n");
      }

      /* BH1750 */
      if (bh1750_init(&g_bh1750_dev, &hi2c1, BH1750_I2C_ADDR_DEFAULT,
                      BH1750_MODE_CONT_HIGH_RES) != ERR_NONE)
      {
          debug_log("BH1750 init failed\n");
      }

      /* DS18B20 */
      Ds18b20Config_t ds18_cfg = {
          .port = DS18B20_GPIO_Port,
          .pin  = DS18B20_Pin
      };
      if (ds18b20_init(&g_ds18b20_handle, &ds18_cfg) != ERR_NONE)
      {
          debug_log("DS18B20 init failed\n");
      }

      /* Relay driver */
      RelayPin_t pump_pin = { .port = RELAY_PUMP_GPIO_Port, .pin = RELAY_PUMP_Pin };
      RelayPin_t mist_pin = { .port = RELAY_MIST_GPIO_Port, .pin = RELAY_MIST_Pin };
      if (relay_init(&g_relay_handle, pump_pin, mist_pin) != ERR_NONE)
      {
          debug_log("Relay init failed\n");
      }

      /* ---- Service layers initialisation ---- */
      /* Sensor Manager */
      SensorManagerConfig_t sensor_cfg = {
          .dht11_handle   = &g_dht_handle,
          .dht11_read     = dht11_adapter_read,
          .bh1750_handle  = &g_bh1750_dev,
          .bh1750_read    = bh1750_adapter_read,
          .ds18b20_handle = &g_ds18b20_handle,
          .ds18b20_read   = ds18b20_adapter_read,
          .soil_handle    = NULL,          /* unused */
          .soil_read      = soil_adapter_read
      };
      if (sensor_manager_init(&g_sensor_mgr, &sensor_cfg) != ERR_NONE)
      {
          debug_log("Sensor Manager init failed\n");
      }

      /* Relay Manager */
      if (relay_manager_init(&g_relay_mgr, &g_relay_handle, 0U) != ERR_NONE)
      {
          debug_log("Relay Manager init failed\n");
      }

      /* RTC Manager */
      if (rtc_manager_init(&g_rtc_mgr, &hrtc) != ERR_NONE)
      {
          debug_log("RTC Manager init failed\n");
      }

      /* Schedule Manager */
      if (schedule_manager_init(&g_schedule_mgr, default_schedule, SCHEDULE_ENTRIES) != ERR_NONE)
      {
          debug_log("Schedule Manager init failed\n");
      }

      /* Initialise last-date for schedule daily reset */
      (void)rtc_manager_get_date(&g_rtc_mgr, &g_last_date);

      /* ---- Application layers initialisation ---- */
      /* Control Manager */
      ControlManagerConfig_t ctrl_cfg = {
          .sensor_mgr   = &g_sensor_mgr,
          .relay_mgr    = &g_relay_mgr,
          .schedule_mgr = &g_schedule_mgr,
          .rtc_mgr      = &g_rtc_mgr
      };
      if (control_manager_init(&g_control_mgr, &ctrl_cfg, MODE_AUTO) != ERR_NONE)
      {
          debug_log("Control Manager init failed\n");
      }

      /* Display Manager (using SH1106 driver functions) */
      g_display_driver.handle    = NULL;   /* SH1106 uses global state */
      g_display_driver.clear     = (disp_clear_fn_t)sh1106_fill;   /* clear frame buffer */
      g_display_driver.draw_str  = (disp_draw_str_fn_t)sh1106_puts;/* draw string */
      g_display_driver.draw_hline= NULL;   /* not used */
      g_display_driver.refresh   = (disp_refresh_fn_t)sh1106_update_screen;

      if (sh1106_init() != ERR_NONE)
      {
          debug_log("SH1106 init failed\n");
      }
      if (display_manager_init(&g_display_mgr, &g_display_driver, 500U) != ERR_NONE)
      {
          debug_log("Display Manager init failed\n");
      }
      display_manager_show_splash(&g_display_mgr);

      /* Communication Manager */
      if (comm_manager_init(&g_comm_mgr, &huart2, on_command_received, NULL) != ERR_NONE)
      {
          debug_log("Comm Manager init failed\n");
      }

      debug_log("System ready.\n");

      /* ---- Main super-loop ---- */
      uint32_t last_sensor_read_ms = 0U;
      uint32_t last_comm_process_ms = 0U;
      uint32_t now;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  now = timing_get_ms();

	  /* 1. Read sensors periodically */
	  if ((now - last_sensor_read_ms) >= SENSOR_READ_PERIOD_MS)
	  {
		  last_sensor_read_ms = now;

	      /* Read all sensors */
	      (void)sensor_manager_read_all(&g_sensor_mgr);

	      /* Run control logic */
	      control_manager_update(&g_control_mgr);

	      /* Get relay status */
	      (void)relay_manager_get_status(&g_relay_mgr, &g_relay_status);

	      /* Get current time */
          (void)rtc_manager_get_time(&g_rtc_mgr, &g_current_time);

          /* Check date rollover and reset schedule state at midnight */

		  DateOfDay_t today;
		  if (rtc_manager_get_date(&g_rtc_mgr, &today) == ERR_NONE)
		  {
			  if ((today.day != g_last_date.day) ||
					  (today.month != g_last_date.month) ||
					  (today.year != g_last_date.year))
			  {
				  schedule_manager_reset_daily(&g_schedule_mgr);
				  g_last_date = today;
				  debug_log("schedule_manager: daily reset (new day %u-%u-%u)\n",
											(unsigned)today.year,
											(unsigned)today.month,
											(unsigned)today.day);
			  }
		  }
	  }

      /* Update relay states and timings (pulse / hysteresis) */
	  relay_manager_update(&g_relay_mgr);
	  (void)relay_manager_get_status(&g_relay_mgr, &g_relay_status);

	  /* 2. Process incoming UART commands (non-blocking) */
	  if ((now - last_comm_process_ms) >= 10U)   /* check every 10 ms */
	  {
		  last_comm_process_ms = now;
	      comm_manager_process(&g_comm_mgr);
	  }

	  /* 3. Update display (rate-limited by display_manager) */
	  {
	  const SensorData_t *sensor_data;
	  ControlMode_t mode;
	  (void)sensor_manager_get_data(&g_sensor_mgr, &sensor_data);
	  (void)control_manager_get_mode(&g_control_mgr, &mode);

	  display_manager_update(&g_display_mgr,
	                                     sensor_data,
	                                     &g_relay_status,
	                                     mode,
	                                     &g_current_time,
										 &g_last_date);
	   }

	   /* 4. Send sensor data over UART (periodic, every sensor read period) */
	   if ((now - last_sensor_read_ms) < 50U)   /* just after sensor read */
	          {
	              const SensorData_t *sensor_data;
	              ControlMode_t mode;
	              (void)sensor_manager_get_data(&g_sensor_mgr, &sensor_data);
	              (void)control_manager_get_mode(&g_control_mgr, &mode);

	              (void)comm_manager_send_data(&g_comm_mgr, sensor_data,
	                                           &g_relay_status, mode);
	   }

	   /* 5. Small delay to let other tasks run */
	   HAL_Delay(MAIN_LOOP_DELAY_MS);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 12;
  RCC_OscInitStruct.PLL.PLLN = 96;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
/* ============================================================================
 *   ADAPTER FUNCTIONS (for SensorManager)
 * ============================================================================ */

/**
 * @brief  Adapter for DHT11 – reads temperature and humidity.
 */
static SensorError_t dht11_adapter_read(void *handle, float *temp_out, float *hum_out)
{
    if ((handle == NULL) || (temp_out == NULL) || (hum_out == NULL))
    {
        return SENSOR_NOT_FOUND;
    }

    DhtHandle_t *dht = (DhtHandle_t *)handle;
    DhtReading_t reading;

    DhtStatus_t status = dht_read(dht, &reading);
    if (status == DHT_OK)
    {
        *temp_out = (float)reading.temperature_x10 / 10.0f;
        *hum_out  = (float)reading.humidity_x10    / 10.0f;
        return SENSOR_OK;
    }
    else if (status == DHT_ERR_TIMEOUT)
    {
        return SENSOR_TIMEOUT;
    }
    else if (status == DHT_ERR_CHECKSUM)
    {
        return SENSOR_CRC_ERROR;
    }
    else
    {
        return SENSOR_NOT_FOUND;
    }
}

/**
 * @brief  Adapter for BH1750 – reads illuminance.
 */
static SensorError_t bh1750_adapter_read(void *handle, float *lux_out)
{
    if ((handle == NULL) || (lux_out == NULL))
    {
        return SENSOR_NOT_FOUND;
    }

    Bh1750_t *dev = (Bh1750_t *)handle;
    ErrorCode_t err = bh1750_read_lux(dev, lux_out);
    if (err == ERR_NONE)
    {
        return SENSOR_OK;
    }
    return SENSOR_TIMEOUT;
}

/**
 * @brief  Adapter for DS18B20 – reads soil temperature.
 */
static SensorError_t ds18b20_adapter_read(void *handle, float *temp_out)
{
    if ((handle == NULL) || (temp_out == NULL))
    {
        return SENSOR_NOT_FOUND;
    }

    Ds18b20Handle_t *dev = (Ds18b20Handle_t *)handle;
    SensorError_t err = ds18b20_read_blocking(dev, temp_out);
    return err;
}

/**
 * @brief  Adapter for soil moisture (ADC) – reads LM393 sensor.
 */
static SensorError_t soil_adapter_read(void *handle, float *moisture_out)
{
    (void)handle;   /* unused – ADC is global */

    if (moisture_out == NULL)
    {
        return SENSOR_NOT_FOUND;
    }

    /* Start ADC conversion (blocking) */
    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 100U) != HAL_OK)
    {
        HAL_ADC_Stop(&hadc1);
        return SENSOR_TIMEOUT;
    }

    uint32_t adc_raw = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);

    /* Convert raw to percentage (0–100%) – calibration values from app_defs.h */
    const uint32_t adc_dry = 3000U;   /* measured in air */
    const uint32_t adc_wet = 1000U;   /* measured in water */

    float percent;
    if (adc_raw >= adc_dry)
    {
        percent = 0.0f;
    }
    else if (adc_raw <= adc_wet)
    {
        percent = 100.0f;
    }
    else
    {
        percent = ((float)(adc_dry - adc_raw) / (float)(adc_dry - adc_wet)) * 100.0f;
    }

    /* Clamp */
    if (percent < 0.0f)   percent = 0.0f;
    if (percent > 100.0f) percent = 100.0f;

    *moisture_out = percent;
    return SENSOR_OK;
}
/* ============================================================================
 *   COMMUNICATION CALLBACK (from ESP32)
 * ============================================================================ */

/**
 * @brief  Callback invoked by communication_manager when a valid command
 *         is received from the ESP32.
 */
static void on_command_received(const ParsedCommand_t *cmd, void *user_data)
{
    (void)user_data;   /* unused */

    switch (cmd->type)
    {
        case CMD_PUMP_ON:
            debug_log("CMD: Pump ON\n");
            /* Ensure manual mode active so manual_set is accepted */
            (void)control_manager_set_mode(&g_control_mgr, MODE_MANUAL);
            (void)control_manager_manual_set(&g_control_mgr, RELAY_ID_PUMP, RELAY_ON);
            break;

        case CMD_PUMP_OFF:
            debug_log("CMD: Pump OFF\n");
            (void)control_manager_set_mode(&g_control_mgr, MODE_MANUAL);
            (void)control_manager_manual_set(&g_control_mgr, RELAY_ID_PUMP, RELAY_OFF);
            break;

        case CMD_MIST_ON:
            debug_log("CMD: Mist ON\n");
            (void)control_manager_set_mode(&g_control_mgr, MODE_MANUAL);
            (void)control_manager_manual_set(&g_control_mgr, RELAY_ID_MIST, RELAY_ON);
            break;

        case CMD_MIST_OFF:
            debug_log("CMD: Mist OFF\n");
            (void)control_manager_set_mode(&g_control_mgr, MODE_MANUAL);
            (void)control_manager_manual_set(&g_control_mgr, RELAY_ID_MIST, RELAY_OFF);
            break;

        case CMD_AUTO_ENABLE:
            debug_log("CMD: Auto enable\n");
            (void)control_manager_set_mode(&g_control_mgr, MODE_AUTO);
            break;

        case CMD_AUTO_DISABLE:
            debug_log("CMD: Auto disable → Manual\n");
            (void)control_manager_set_mode(&g_control_mgr, MODE_MANUAL);
            break;

        case CMD_SET_TIME:
            debug_log("CMD: Set time %02u:%02u:%02u\n", cmd->hour, cmd->minute, cmd->second);
            {
                TimeOfDay_t t = { .hour = cmd->hour, .minute = cmd->minute, .second = cmd->second };
                (void)rtc_manager_set_time(&g_rtc_mgr, &t);
            }
            break;

        default:
            debug_log("CMD: Unknown (ignored)\n");
            break;
    }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
      LL_GPIO_TogglePin(LED_B_GPIO_Port, LED_B_Pin);
      HAL_Delay(200);
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
