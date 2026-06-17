# Wing Drum — Linux Build Setup Guide

This document describes how to set up the ESP-IDF toolchain on a Linux machine and build the Wing Drum firmware from source.

Tested on: **Debian 13 (trixie), x86_64**, with **ESP-IDF v4.4.7** and **Python 3.13**.

---

## 1. System Prerequisites

Install the packages required by ESP-IDF before anything else:

```bash
sudo apt-get update
sudo apt-get install -y \
    git wget flex bison gperf cmake ninja-build ccache \
    libffi-dev libssl-dev dfu-util libusb-1.0-0-dev \
    python3-pip python3-venv python3-setuptools python3-virtualenv
```

**Why `python3-virtualenv`?**  
Debian 13 enforces [PEP 668](https://peps.python.org/pep-0668/), which prevents `pip` from installing packages into the system Python environment. ESP-IDF's installer (`idf_tools.py`) needs the `virtualenv` package to create its own isolated Python environment. Installing it via `apt` rather than `pip` satisfies the requirement without breaking system Python.

---

## 2. Clone ESP-IDF

The firmware targets **ESP-IDF v4.4**. Tag `v4.4.7` (released September 2023) is the recommended stable release for this project.

```bash
mkdir -p ~/esp
git clone --recursive --branch v4.4.7 --depth 1 \
    https://github.com/espressif/esp-idf.git ~/esp/esp-idf
```

`--recursive` is required: ESP-IDF has many submodules (mbedtls, lwip, openthread, etc.) that must be checked out alongside the main repository.

`--depth 1` keeps the clone shallow to save disk space and download time.

---

## 3. Install ESP-IDF Tools

Run the official tool installer, specifying the `esp32` target so it only downloads what is needed for this chip (avoids fetching toolchains for ESP32-S2, S3, C3, etc.):

```bash
cd ~/esp/esp-idf
./install.sh esp32
```

This script downloads and installs the following into `~/.espressif/`:

| Tool | Version | Purpose |
|------|---------|---------|
| `xtensa-esp32-elf-gcc` | 8.4.0 (esp-2021r2-patch5) | C/C++ cross-compiler for Xtensa LX6 |
| `xtensa-esp-elf-gdb` | 11.2_20220823 | Debugger |
| `esp32ulp-elf` | 2.35_20220830 | ULP co-processor assembler |
| `openocd-esp32` | v0.12.0-esp32-20230921 | On-chip debugger interface |
| Python venv | idf4.4_py3.13_env | Isolated Python env with IDF packages |

Installation takes a few minutes on first run. Subsequent runs are fast (tools are cached).

---

## 4. Set Up the Environment

ESP-IDF tools are not on `PATH` by default. Source the export script at the start of every build session:

```bash
source ~/esp/esp-idf/export.sh
```

You can verify the setup with:

```bash
idf.py --version        # should print: ESP-IDF v4.4.7
xtensa-esp32-elf-gcc --version  # should print: gcc 8.4.0
```

**Optional — add to your shell profile:**  
If you work with this project frequently, add an alias to `~/.bashrc` or `~/.zshrc`:

```bash
alias get_idf='source ~/esp/esp-idf/export.sh'
```

Then run `get_idf` before starting a build session.

---

## 5. Known Source Fix

There is a filename case mismatch in the original source that prevents compilation on case-sensitive Linux filesystems (the file on disk is `Reverb.h` but the include used a lowercase `r`).

**File:** `main/hw/signals.h`, line 26

```c
// Before (broken on Linux):
#include "dsp/reverb.h"

// After (correct):
#include "dsp/Reverb.h"
```

This fix is already applied in this branch. No action needed.

---

## 6. Build the Firmware

```bash
cd /path/to/wingdrum
idf.py build
```

A successful build ends with output similar to:

```
Project build complete. To flash, run this command:
python esptool.py -p (PORT) -b 460800 ... write_flash ...
```

Build artefacts are written to `build/`:

| File | Description |
|------|-------------|
| `build/app-template.bin` | Main application binary |
| `build/bootloader/bootloader.bin` | ESP32 second-stage bootloader |
| `build/partition_table/partition-table.bin` | Partition table |

---

## 7. Flash to the Device

Connect the Wing Drum board via USB, identify the serial port (typically `/dev/ttyUSB0` or `/dev/ttyACM0`), then:

```bash
idf.py -p /dev/ttyUSB0 flash
```

To also open the serial monitor after flashing:

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

Press `Ctrl+]` to exit the monitor.

**Permissions note:** On Debian/Ubuntu, your user must be in the `dialout` group to access serial ports without `sudo`:

```bash
sudo usermod -aG dialout $USER
# Log out and back in for the change to take effect
```

---

## 8. Cleaning the Build

To force a full rebuild (e.g., after changing `sdkconfig` options):

```bash
idf.py fullclean
idf.py build
```

To open the interactive configuration menu:

```bash
idf.py menuconfig
```

---

## References

- [ESP-IDF v4.4 Getting Started (Linux)](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32/get-started/linux-macos-setup.html)
- [Wing Drum product page](https://phonicbloom.com/drum/)
- ESP-IDF commit used during development: `a49e0180ee638e41876a8f7bc6428a983fc69d66` (18 Aug 2023)
