SmartFarm_STM32
================

Overview
--------
Firmware for SmartFarm monitoring and actuation on STM32F411CEU6.
Layers: Device → Services → Application (control, schedule, display, comms).

Quick wiring
------------
- I2C1 (PB6=SCL, PB7=SDA): BH1750 + SH1106 (3.3V)
- OneWire DHT11: PB5 (4.7k pull-up)
- OneWire DS18B20: PB4 (4.7k pull-up)
- ADC Soil sensor: PA1
- UART2 -> ESP32: PA2 (TX), PA3 (RX) — 115200 baud
- Relay pump: PB0 (active LOW)
- Relay mist: PB1 (active LOW)

Build & Flash
-------------
Recommended: STM32CubeIDE (import CubeMX project).
Alternatively, use makefile in `Debug/` (project configured with CubeMX). Typical steps:

```bash
# build (example using makefile)
cd Debug
make all

# flash via ST-Link (example)
st-flash write Debug/SmartFarm_STM32.bin 0x8000000
```

Notes on changes made
---------------------
I applied the following fixes requested during review:

- `Common/Src/app_config.c`: initialised the `weekday_mask` field for each `default_schedule` entry so the schedule manager has valid masks (Mon–Fri and Sat–Sun).

- `Core/Src/main.c`: added `g_last_date` and code to detect midnight/date rollover and call `schedule_manager_reset_daily()` so scheduled entries fire again each day.

- `App/Src/control_manager.c`: ensure `mgr->heat.active` is set when heat-protection engages and cleared when it ends; track pulse start so `relay_manager_pulse()` is invoked on entering heat mode.

- `App/Src/communication_manager.c`: expose a global `g_comm_manager_global` and set it during `comm_manager_init()` to allow HAL callbacks to forward received bytes.

- `Core/Src/usart.c`: added `HAL_UART_RxCpltCallback()` forwarding to `comm_manager_rx_isr_callback()` when `USART2` activity completes. This provides a default integration path for RX events.

Integration notes / TODOs
------------------------
- UART RX mode: This project configures USART2 with DMA in CubeMX. The minimal forwarder added in `usart.c` will work when `HAL_UART_RxCpltCallback()` is called (e.g. single-byte or DMA completion). For robust continuous RX consider using:
  - Circular DMA + IDLE-line detection, then handle IDLE in `USARTx_IRQHandler` and call `comm_manager_process()` from the main loop, or
  - HAL UART RX interrupt (`HAL_UART_Receive_IT`) for single-byte reception as the communication manager currently expects.

- Manual commands: `on_command_received()` now sets the controller to `MODE_MANUAL` before forwarding manual ON/OFF commands so ESP32 commands take effect even when system is in AUTO.

- `schedule_manager_get_next()` is now implemented for next watering lookup on the current day.

- Add hardware schematics and a README picture for wiring if submitting the project.

If you want, I can:
- Run a targeted search for remaining unchecked references and patch them similarly.
- Implement `schedule_manager_get_next()`.
- Add a small `docs/` folder with a wiring diagram and example UART logs.


Changelog
---------
See commits in this working tree. These changes are minimal and focused on correctness and integration rather than refactoring.
