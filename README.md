# bike_nav (ESP32-S3 Firmware)

Firmware project for the **Waveshare ESP32-S3-Touch-LCD-1.85B** using **ESP-IDF v6.1** on Ubuntu with VS Code.

## Hardware target

- Board: Waveshare ESP32-S3-Touch-LCD-1.85B
- MCU target: `esp32s3`
- Connection: native USB-C (USB Serial/JTAG)
- Linux serial device: `/dev/ttyACM0`

## Required environment

- Ubuntu (development host)
- VS Code
- Espressif IDF extension for VS Code
- ESP-IDF **v6.1**
- Python + ESP-IDF toolchain managed by the extension

## Understanding Your Terminals

- **Standard Bash Terminal:** Standard system shell; does **not** have Espressif tools in its PATH by default.
- **ESP-IDF Terminal:** Open this via the VS Code command palette (`ESP-IDF: Open ESP-IDF Terminal`) or extension icon. It automatically loads the ESP-IDF toolchain environment. **Always run your `idf.py` or `make` commands in this terminal.**

## Daily Development Workflow (Step-by-Step)

1. **Plug in your board** via USB-C.
2. Open VS Code and ensure you are in an **ESP-IDF Terminal** inside the project root folder.
3. If you just plugged the device in and encounter a port permission error (`/dev/ttyACM0 is not readable`), run these quick commands to release locks and set permissions:
   ```bash
   sudo fuser -k /dev/ttyACM0
   sudo chmod 666 /dev/ttyACM0
   ```
4. Build, flash, and monitor using the project Makefile shortcuts:
   ```bash
   make all      # Cleans, targets, builds, and flashes in one go
   make monitor  # Opens the live serial logs
   ```

### Makefile Reference

| Command | Action Equivalent |
| :--- | :--- |
| `make build` | `idf.py build` |
| `make clean` | `idf.py fullclean` |
| `make target` | `idf.py set-target esp32s3` |
| `make flash` | Flashes binaries via `esptool` to `/dev/ttyACM0` |
| `make monitor`| Opens the serial log monitor |
| `make all` | Executes clean, build, and flash sequentially |

## First-time project setup

From the project root (in an ESP-IDF Terminal):

```bash
idf.py set-target esp32s3
idf.py build
```

## Current launcher behavior

The firmware currently boots into a basic modular launcher flow:

0. Board display stack is initialized for this hardware:
   - LCD: ST77916 over QSPI
   - Touch: CST816S over I2C

1. A **black splash screen** is shown with centered **HELLO** text that fades in, then fades out (~2 seconds total).
2. The **home screen** appears and shows:
   - Current time (top-left)
   - Battery percentage (top-right)
   - Black background with launcher area/message in the center
3. If no apps are registered, the home screen shows **"No apps available"**.
4. If apps are registered, users can:
   - **Swipe left/right** on the app card to move between apps
   - **Tap** the card to open the selected app
   - Tap again on the opened app screen to return home

### Adding modular apps

Apps are defined in [app_registry.c](./main/app_registry.c) using the `bike_app_t` structure declared in [app_registry.h](./main/app_registry.h).  
To add a new app, add an entry to the `APPS[]` array with:
- `id`: unique app id
- `name`: display name
- `icon`: short icon text for the launcher card
- `on_open`: callback executed when the app is opened

## Flashing notes & troubleshooting hotplug issues

- Make sure the board is connected over USB-C and visible as `/dev/ttyACM0`.
- **One-time user group setup:** Ensure your user account belongs to the `dialout` group so Linux grants serial access:
  ```bash
  sudo usermod -aG dialout $USER
  ```
  *(Remember to restart your terminal or run `newgrp dialout` after executing this).*
- **Hotplug permission reset fix:** Every time the USB-C cable is unplugged and reinserted, Ubuntu recreates the device file. If flashing complains that the port is not readable, run:
  ```bash
  sudo chmod 666 /dev/ttyACM0
  ```
