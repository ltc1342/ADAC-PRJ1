# PROJECT: Smart Farm Monitoring

- Programming Software: |----STM32CubeIDE for STM32
                        |----PlatformIO on Visual Studio Code for ESP32

- Hardware:             |----STM32F411CEU6 (Black Pill) --> Main Microcontroller (Task processing)
                        |----ESP32 WROOM DevKit (Xtensa) --> Sub-microcontroller (Connect Wifi send data to MQTT)

- Peripherals for STM32 |----Oled Screen (SH1106) communication I2C
                        |----Temperature and humidity sensor (DHT11) communication OneWire
                        |----Light sensor (GY-302 BH1750) communication I2C
                        |----Soil moisture sensor (SMS-V1 LM393) communication ADC
                        |----ESP32 WROOM DevKit communication UART
                        |----Module 2 5V Low Level Triggered Relays communication GPIO (Output)

# Technical Specifications - Smart Farm Hardware

## 1. Main Microcontroller: STM32F411CEU6 (Black Pill)

| Parameter                     |Value                                                  |
|-------------------------------|-------------------------------------------------------|
| **Microcontroller**           | Original STM32F411CEU6 chip |
| **Core**                      | ARM® 32-bit Cortex® -M4 CPU with FPU |
| **Operating Voltage (VDD)**   | 1.7V ~ 3.6V |
| **Input Supply Voltage**      | 3.3V ~ 5V |
| **Max CPU Frequency**         | 100 MHz |
| **Flash Memory**              | 512 KB |
| **SRAM**                      | 128 KB |
| **Crystal Oscillators**       | 25 MHz (high-frequency) & 32.768 kHz (low-frequency) |
| **Interfaces**                | USART x3, I2C x3, SPI x5, USBFS x1, ADC x1 (10 channels), I2S x5, TIM x8 |
| **On-board Components**       | Reset button, BOOT0 button, user button |
| **PCB Dimensions**            | 20.78mm x 52.81mm |
| **Connector Pitch**           | 2.54mm |

**Key Features**:
- UFQFPN48 package
- Supports development in C language
- USB Type-C Connector
- 20-pin I/O interface + 4-pin debugging interface
- Max output current (3.3V LDO): 100 mA

> Sources: [https://stm32-base.org/boards/STM32F411CEU6-WeAct-Black-Pill-V2.0.html]

---

## 2. Sub-microcontroller (Wi-Fi): ESP-WROOM-32

| Parameter                 |Value                                                  |
|---------------------------|-------------------------------------------------------|
| **SoC**                   | ESP32-D0WDQ6 (or ESP32-D0WD-V3) |
| **Processor**             | Xtensa® dual-core 32-bit LX6 microprocessor |
| **Clock Frequency**       | Up to 240 MHz |
| **Operating Voltage**     | 2.7V ~ 3.6V (typical 3.0V ~ 3.6V) |
| **ROM**                   | 448 KB |
| **SRAM**                  | 520 KB |
| **External Flash**        | 4 MB SPI flash |
| **Wi-Fi**                 | 802.11 b/g/n, up to 150 Mbps |
| **Bluetooth**             | V4.2 BR/EDR and BLE |
| **Peripheral Interfaces** | SD card, UART, SPI, SDIO, I2C, LED PWM, Motor PWM, I2S, IR, pulse counter, GPIO, capacitive touch sensor, ADC, DAC, TWAI (CAN 2.0) |
| **Antenna**               | On-board PCB antenna |
| **Operating Temperature** | -40 ~ 85 °C |
| **Crystal Oscillator**    | 40 MHz |

**Key Features**:
- Class-1, class-2 and class-3 transmitter support
- 32 GPIOs (5 strapping GPIOs)
- 8 KB SRAM in RTC
- Certified: REACH/RoHS

> Sources: [https://www.upesy.com/blogs/tutorials/esp32-pinout-reference-gpio-pins-ultimate-guide#]
---

## 3. Display: OLED Screen (SH1106)

| Parameter | Value |
|-----------|-------|
| **Driver Chip** | SH1106 |
| **Communication Interface** | I2C, 3-wire SPI, 4-wire SPI |
| **Resolution** | 128 x 64 pixels |
| **Display Size** | 1.3 inch (typical) |
| **Operating Voltage** | 3.3V / 5V |
| **Display Color** | Blue (or white/yellow depending on variant) |
| **Visible Angle** | > 160° |
| **Operating Temperature** | -20 ~ 70 °C |
| **Storage Temperature** | -30 ~ 80 °C |
| **Outline Dimensions** | 65mm x 30mm (typical for 1.3" module) |
| **Fast Response Time** | 10μs even at -40°C |

**Pin Functions (I2C mode)**:
- `VCC` → Power supply (3.3V/5V)
- `GND` → Ground
- `SCL` → I2C Clock
- `SDA` → I2C Data

**Notes**:
- Modules may include additional pins for DC (Data/Command), CS (Chip Select), and RST (Reset)
- For I2C communication, specific resistors need to be soldered on the back of the module according to the communication mode configuration

> Sources: [reference:3]

---

## 4. Temperature and Humidity Sensor: DHT11

| Parameter | Value |
|-----------|-------|
| **Communication Protocol** | OneWire (single bus digital interface) |
| **Operating Voltage** | 3V ~ 5.5V |
| **Temperature Range** | 0°C ~ 50°C (or -20°C ~ 60°C depending on specification) |
| **Temperature Accuracy** | ±2°C |
| **Temperature Resolution** | 1°C |
| **Humidity Range** | 20% ~ 90% RH (or 5% ~ 95% RH depending on specification) |
| **Humidity Accuracy** | ±5% RH |
| **Humidity Resolution** | 1% RH |
| **Sampling Period** | > 2 seconds |

**Key Features**:
- Composite sensor with calibrated digital signal output
- Contains capacitive humidity sensing element and NTC temperature measuring element
- Connected to a high-performance 8-bit microcontroller
- Data packet structure: humidity integer, humidity decimal, temperature integer, temperature decimal, and checksum

> Sources: [reference:4]

---

## 5. Light Sensor: GY-302 (BH1750)

| Parameter | Value |
|-----------|-------|
| **Model** | GY-302 |
| **Chip** | BH1750FVI (ROHM original package) |
| **Communication Interface** | I2C |
| **Power Supply** | 3V ~ 5V |
| **Illuminance Range** | 0 ~ 65535 Lux |
| **Built-in ADC** | 16-bit Analog-to-Digital Converter |
| **Dimensions** | 13.9mm x 18.5mm |
| **Pin Header Pitch** | 2.54mm (5-pin accessible) |

**Pin Functions**:
- `VCC` → Power supply (3~5V)
- `GND` → Ground
- `SCL` → I2C Clock
- `SDA` → I2C Data
- `ADDR` → I2C address selection pin

**Key Features**:
- Direct digital output → no complex calculations or calibration required
- High precision accurate to 1 Lux for different light sources
- Spectral response close to human visual sensitivity
- No external parts required

> Sources: [reference:5]

---

## 6. Soil Moisture Sensor: SMS-V1 LM393

| Parameter | Value |
|-----------|-------|
| **Sensor Type** | Resistive (2-probe blade) |
| **Operating Voltage** | 3.3V ~ 5V DC |
| **Operating Current** | < 20 mA (typical 15 mA) |
| **Comparator IC** | LM393 (on-board, threshold adjustable via potentiometer) |
| **Analog Output (AO)** | ~0V (wet) to ~VCC (dry) |
| **Digital Output (DO)** | HIGH/LOW based on preset threshold (adjustable) |
| **Probe Length** | ~60 mm |
| **Probe Material** | Tinned copper (corrosion-prone) |
| **PCB Dimensions** | ~30 x 16 mm |

**Key Features**:
- Dual output mode: analog output for precise measurements, digital output for simplicity
- On-board potentiometer for sensitivity adjustment
- Power indicator (red) and digital switching output indicator (green)
- A fixed bolt hole for easy installation

**Important Notes**:
- Resistive probes corrode over time when continuously powered in damp soil
- For long-term deployment, power the sensor only when taking readings (use GPIO to switch VCC)
- Operating temperature: 10 ~ 30 °C (typical)

> Sources: [reference:6]

---

## 7. Relay Module: 5V Low Level Triggered Relay (GPIO Output)

| Parameter | Value |
|-----------|-------|
| **Operating Voltage** | 5V DC |
| **Trigger Type** | Low Level Trigger (Active LOW) |
| **Relay Type** | SPDT (Single Pole Double Throw) |
| **Contact Rating (AC)** | 10A at 250V AC |
| **Contact Rating (DC)** | 10A at 30V DC (or up to 30A for some variants) |
| **Trigger Voltage** | 2V ~ 5V |
| **Quiescent Current** | ~4 mA |
| **Working Current** | ~65 mA (triggered) |
| **Isolation** | Optocoupler based electrical isolation |

**Key Features**:
- LED indicators for power and relay status
- Standard screw terminals for easy load connections
- One normally closed (NC) contact and one normally open (NO) contact
- Pull-down circuit to avoid malfunction
- Triode drive to increase relay coil stability

**Dimensions (typical)**:
- Length: ~54 mm
- Width: ~20 mm
- Height: ~18 mm

> Sources: [reference:7]

---

## Communication Overview

| Connection | Between | Protocol / Interface |
|------------|---------|----------------------|
| STM32 ↔ OLED SH1106 | Main ↔ Peripheral | I2C |
| STM32 ↔ DHT11 | Main ↔ Peripheral | OneWire |
| STM32 ↔ BH1750 (GY-302) | Main ↔ Peripheral | I2C |
| STM32 ↔ Soil Moisture Sensor | Main ↔ Peripheral | ADC (Analog-to-Digital) |
| STM32 ↔ Relay Module | Main ↔ Peripheral | GPIO (Digital Output) |
| STM32 ↔ ESP32 | Main ↔ Sub | UART |

---

## Important Notes for Integration

1. **Power Supply Compatibility**:
   - All peripherals operate at either 3.3V or 5V
   - STM32F411CEU6 I/O pins are 3.3V tolerant (but can accept 5V on dedicated power pins)
   - ESP32 I/O pins are 3.3V only

2. **Soil Moisture Sensor Longevity**:
   - Resistive probes corrode quickly when continuously powered in moist soil
   - For long-term farming applications, power the sensor only during reading intervals

3. **Relay Module**:
   - Low level trigger means the relay activates when the control pin is pulled LOW
   - Optocoupler isolation provides protection for sensitive microcontroller circuits