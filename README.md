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

## VS Code + ESP-IDF extension setup

1. Install the **Espressif IDF** extension in VS Code.
2. Open this project folder in VS Code.
3. Run command palette: **ESP-IDF: Configure ESP-IDF extension**.
4. Select or install **ESP-IDF v6.1**.
5. Ensure the extension Python/toolchain install completes.
6. Open an **ESP-IDF Terminal** in VS Code (this loads the IDF environment).

> Use the ESP-IDF terminal (or source `export.sh`) before running `idf.py` or `make` commands.

## First-time project setup

From the project root:

```bash
idf.py set-target esp32s3
idf.py build
```

## Daily development workflow

### Option A: Direct `idf.py`

```bash
idf.py build
idf.py -p /dev/ttyACM0 flash
idf.py -p /dev/ttyACM0 monitor
```

### Option B: Project Makefile shortcuts

Use the provided [Makefile](</home/mhubbard/Dev/personal/Bike nav/bike-nav-firmware/Makefile>):

```bash
make build      # idf.py build
make clean      # idf.py fullclean
make target     # idf.py set-target esp32s3
make flash      # flash to /dev/ttyACM0
make monitor    # serial monitor on /dev/ttyACM0
make all        # clean -> target -> build -> flash
```

## Flashing notes (ESP32-S3 via native USB-C)

- Make sure the board is connected over USB-C and visible as `/dev/ttyACM0`.
- If permissions fail on `/dev/ttyACM0`, add your user to `dialout` and re-login:

```bash
sudo usermod -aG dialout $USER
```

## Useful clean/rebuild flow

```bash
make clean
make all
make monitor
```

