# STM32 Tasks – Smart Farm Monitoring (STM32F411CEU6)

## 1. System Overview
- **Main MCU**: STM32F411CEU6 (Black Pill)
- **Sensors**:
  - DHT11 (Temperature & Humidity) – OneWire
  - Soil Moisture Sensor (LM393) – ADC
  - BH1750 (Light intensity) – I2C
- **Actuators**:
  - 2x Relay modules (5V low‑level trigger) – GPIO
    - Relay 1: Water pump (soil moisture control)
    - Relay 2: Mist maker / sprinkler (air humidity control)
- **Display**: SH1106 OLED (128x64) – I2C
- **Communication**: UART2 → ESP32 (baud rate 115200)
- **Power**: 3.3V / 5V (ensure level shifting if needed)

## Software Architecture

The STM32 firmware is designed using a layered architecture to improve maintainability, scalability, and future migration to FreeRTOS.

```text
Device Layer
│
├── DHT11 / DHT22 / SHT40
├── DS18B20
├── BH1750
├── SH1106
└── Relay
│
▼
Service Layer
│
├── Sensor Manager
├── Relay Manager
└── RTC Manager
│
▼
Application Layer
│
├── Control Manager
├── Schedule Manager
├── Display Manager
└── Communication Manager
```

Development Levels describe functional maturity of the project, while the software architecture remains unchanged across all levels.

---

## 2. Hardware Pin Mapping (Suggested)

| Peripheral | STM32 Pin | Notes |
|------------|-----------|-------|
| **I2C1** (OLED + BH1750) | PB6 (SCL), PB7 (SDA) | 3.3V logic |
| **OneWire** (DHT11) | PB5 | External 4.7k pull‑up |
| **OneWire** (DS18B20) | PB4 | External 4.7k pull‑up |
| **ADC1** (Soil moisture) | PA1 | Analog input, 0–3.3V |
| **UART2** → ESP32 | PA2 (TX), PA3 (RX) | 115200 baud |
| **Relay 1** (Pump) | PB0 | Active LOW |
| **Relay 2** (Mist) | PB1 | Active LOW |
| **User button** (optional) | PC13 | For manual override |

> **Note**: DHT11 requires precise timing. Use HAL_Delay or a timer interrupt.

---

## 3. Development Levels & Tasks

### 🔹 Level 1 – Basic Monitoring (Read & Display & Send)
**Goal**: Read all sensors, show on OLED, send raw data to ESP32 every 5 seconds.

| Task | Description |
|------|-------------|
| 1.1 | Initialize system clock (96 MHz), I2C, UART, ADC, GPIO. |
| 1.2 | Write driver for DHT11 (OneWire reset, read 40 bits, checksum). |
| 1.3 | Write driver for BH1750 (I2C commands: Power On, Continuous H‑Resolution Mode). |
| 1.4 | Read soil moisture from ADC (12‑bit, convert to percentage: 0% = dry ~3.3V, 100% = wet ~0V). |
| 1.5 | Display on SH1106 (using u8g2 or Adafruit SSD1306 library ported for STM32). Show: Temp, Hum, Light (Lux), Soil (%). |
| 1.6 | Pack sensor data into a simple string or JSON (e.g., `"T:25.5,H:60,L:320,S:45"`). |
| 1.7 | Send data via UART2 to ESP32 every 5 seconds (use non‑blocking DMA or simple HAL_UART_Transmit). |

### 🔹 Level 2 – Automatic Control (Threshold‑based)
**Goal**: Control water pump and mist maker based on predefined thresholds.

| Task | Description |
|------|-------------|
| 2.1 | Define thresholds in code: e.g., Soil < 40% → pump ON; Soil > 70% → pump OFF. Air humidity < 50% → mist ON; > 70% → mist OFF. |
| 2.2 | Implement simple control loop inside main while(1) – check sensors, compare thresholds, set relay GPIOs. |
| 2.3 | Add 5 second hysteresis to avoid relay toggling too fast. |
| 2.4 | Display relay status on OLED (e.g., pump icon, mist icon). |
| 2.5 | Send control events (ON/OFF) to ESP32 via UART. |
| 2.6 | (Optional) Add a manual override push button (PC13) to force pump ON for 30 seconds. |

### 🔹 Level 3 – Advanced Greenhouse Temperature Control (PID)

**Goal**: Maintain greenhouse air temperature using closed-loop PID control.

#### Sensors

* DHT22 (recommended)
* SHT40 (preferred)

#### Monitoring Parameters

* Air Temperature
* Air Humidity
* Soil Temperature (DS18B20)
* Soil Moisture (LM393)

#### Actuators

* Heating Lamp
* PWM Fan (optional future expansion)

#### Tasks

| Task | Description                                                        |
| ---- | ------------------------------------------------------------------ |
| 3.1  | Replace DHT11 with DHT22 or SHT40 for higher measurement accuracy. |
| 3.2  | Define greenhouse temperature setpoint (e.g. 28°C).                |
| 3.3  | Implement PID controller using floating point arithmetic.          |
| 3.4  | Calculate control output every 2 seconds.                          |
| 3.5  | Drive heating lamp using PWM through MOSFET driver.                |
| 3.6  | Display Setpoint, Process Variable and PID Output on OLED.         |
| 3.7  | Send PID telemetry to ESP32 for monitoring and tuning.             |
| 3.8  | Implement safety limits to prevent overheating.                    |

#### Example

```text
Setpoint = 28°C

Current Temp = 25°C

Error = 3°C

PID Output = 80%

Heating Lamp Duty = 80%
```
---

### 🔹 Level 4 – FreeRTOS Migration & System Optimisation

**Goal**: Convert the firmware from a super-loop architecture into a task-based RTOS architecture.

#### Task Structure

```text
SensorTask
    Read sensors

ControlTask
    Automatic control logic

DisplayTask
    OLED update

CommunicationTask
    UART communication

ScheduleTask
    RTC schedule handling
```

#### Tasks

| Task | Description                                     |
| ---- | ----------------------------------------------- |
| 4.1  | Enable FreeRTOS in STM32CubeMX.                 |
| 4.2  | Convert managers into dedicated RTOS tasks.     |
| 4.3  | Use queues for sensor data exchange.            |
| 4.4  | Use mutexes for shared peripherals (I2C, UART). |
| 4.5  | Implement software timers for periodic actions. |
| 4.6  | Add watchdog supervision.                       |
| 4.7  | Implement low-power modes using RTC wakeup.     |
| 4.8  | Measure CPU load and memory usage.              |

#### Expected Benefits

* Better modularity
* Improved responsiveness
* Easier future feature expansion
* Cleaner migration to larger embedded systems

```
```

---

## 4. Communication Protocol with ESP32

**UART Settings**: 115200 baud, 8 data bits, 1 stop bit, no parity.

### Message Format (Plain string / CSV)
Example:
<timestamp>,<temp>,<hum>,<light>,<soil>,<pump>,<mist>\r\n
1700000,25.3,58,320,45,1,0

- `timestamp` = uptime seconds (optional)
- `pump` = 0/1
- `mist` = 0/1

### Control Commands from ESP32 (Level 2+)
ESP32 can send commands to override relays:
- `PUMP_ON` , `PUMP_OFF`
- `MIST_ON` , `MIST_OFF`
- `AUTO_ENABLE` , `AUTO_DISABLE`

STM32 parses these and acts accordingly.

---

## 5. Recommended Libraries / Tools
- **STM32CubeIDE** with CMSIS drivers.
- **u8g2** or **Adafruit SH1106** library for OLED.
- **FreeRTOS** (included in CubeIDE) for Level 4.
- **STM32 Low Power** (PWR, RTC) for deep sleep.
