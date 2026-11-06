# **Project Hardware Context: Waveshare ESP32-S3-Touch-LCD-1.85B**

This document defines the exact hardware map, pin allocations, and peripheral drivers for the ESP32-S3 development board used in this repository. All AI-generated code must respect these specifications.

## **1\. Core Microcontroller**

* **MCU:** Espressif ESP32-S3 (Dual-core Xtensa LX7 microprocessor)
* **Flash / PSRAM:** 8MB Flash, 8MB PSRAM
* **Development Framework:** ESP-IDF / Arduino-ESP32 using the official Waveshare Board Support Package (esp-bsp).

## **2\. Onboard Peripherals & Chipset Map**

| Peripheral | Chip Model | Communication Bus / Address | Key Notes |
| :---- | :---- | :---- | :---- |
| **Display Touch** | CST816S | I2C | Capacitive touch controller |
| **IMU (Motion)** | QMI8658 | I2C (0x6B) | 6-axis Accelerometer & Gyroscope |
| **Real-Time Clock** | PCF85063A | I2C | Requires 3.7V lithium battery backup on header for persistent timekeeping |
| **Audio Codec** | ES8311 | I2S / I2C | Digital-to-analog audio playback and speaker output |
| **Audio Amplifier** | Hardware PA | GPIO (BSP\_AUDIO\_PA / GPIO9) | Must be driven high (1) to enable speaker output; low (0) to mute/disable |
| **Battery Fuel Gauge** | BQ27220 | I2C | Monitors battery voltage and state-of-charge |
| **Storage** | Micro SD Card | SPI / SDMMC | Local logging and asset storage |

## **3\. Critical Code Patterns & Architecture Rules**

### **Audio & Codec Rules (ES8311 & BSP\_AUDIO\_PA)**

* **Amplifier Control:** Never manually toggle BSP\_AUDIO\_PA (GPIO9) raw directions or states without coordinating with the audio session manager. The audio pipeline expects bsp\_extra\_codec\_init() and bsp\_audio\_codec\_speaker\_init() to handle setup.
* **Session Locking:** Audio playback tasks must acquire the audio session via bsp\_extra\_audio\_session\_acquire(BSP\_EXTRA\_AUDIO\_OWNER\_MUSIC) before opening the codec device to prevent silent task crashes or resource conflicts.
* **Cleanup:** Always mute the codec (esp\_codec\_dev\_set\_out\_mute(codec, true)), close it, set the PA pin low, and release the audio session before deleting audio tasks.

### **IMU Sensor Rules (QMI8658)**

* Always initialize the I2C master bus via bsp\_i2c\_get\_handle() before calling qmi8658\_init().
* Configure accelerometer range (e.g., QMI8658\_ACCEL\_RANGE\_8G) and ODR appropriately before polling data inside periodic timers.
