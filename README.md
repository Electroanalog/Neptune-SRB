<a name="top"></a>

<a href="https://www.electroanalog.com">
  <img src="https://electroanalog.github.io/img/electroanalog_logo.png" alt="Electroanalog" width="270px" />
</a>

# Neptune Smart Reset Button

[![License](https://img.shields.io/github/license/Electroanalog/Neptune-SRB)](LICENSE)
[![Release](https://img.shields.io/github/v/release/Electroanalog/Neptune-SRB)](../../releases)
[![MCU](https://img.shields.io/badge/PIC-16F630|676-yellow)]()
[![Tested on Hardware](https://img.shields.io/badge/Tested-Neptune%2032X%2FMD2-success)]()

Neptune-SRB is a modern replacement for the original **Switchless Mod**, designed for **Cosam Neptune** and fully compatible with **Sega Genesis/Mega Drive**. It introduces redesigned control logic, enhanced reset handling, In-Game Reset (IGR), RGB LED feedback, and 50/60Hz switching while remaining compatible with the original hardware.

## Table of Contents

- [Overview](#overview)
- [Building & Flashing](#building--flashing)
- [Installation Notes](#installation-notes)
- [In-Game Reset (IGR)](#in-game-reset-igr)
- [Configuration](#configuration)
- [About this Guide](#about-this-guide)

---

## Overview

Neptune Smart Reset Button (Neptune-SRB) is a modern replacement for the original Switchless Mod developed for the Cosam Neptune. While preserving the original single-button operating concept, it introduces redesigned control logic, improved usability, and additional functionality.

The firmware supports region selection, 50/60Hz switching, In-Game Reset (IGR), configurable RGB LED feedback, and persistent EEPROM storage for the last selected region. It is fully compatible with both the Cosam Neptune and standard Sega Genesis/Mega Drive systems using the same hardware platform.

Unlike the original implementation, all timing and button handling have been redesigned to provide deterministic behavior, clear visual feedback, and straightforward customization through compile-time configuration.

### PIC Pinout

The firmware is designed for the **PIC16F630** and **PIC16F676**, which share the same pinout and are fully interchangeable.

| Pin | Signal | Description |
|:---:|--------|-------------|
| 1 | **VDD** | +5V Supply |
| 2 | **RA5** | IGR TR input (Controller Port Pin 9) |
| 3 | **RA4** | IGR TL input (Controller Port Pin 6) |
| 4 | **RA3** | IGR UP input (Controller Port Pin 1) / ICSP MCLR/VPP |
| 5 | **RC5** | Green LED output |
| 6 | **RC4** | Red LED output |
| 7 | **RC3** | Not used |
| 8 | **RC2** | Blue LED output |
| 9 | **RC1** | IGR TH input (Controller Port Pin 7) |
| 10 | **RC0** | JAP Region output |
| 11 | **RA2** | RESET output |
| 12 | **RA1** | NTSC (50/60Hz) output / ICSP CLK |
| 13 | **RA0** | RESET button input / ICSP DAT |
| 14 | **VSS** | Ground |

### RGB LED Support

The RGB LED provides immediate visual feedback for the currently selected region and system status. The default color assignment is shown below and can be changed by editing the firmware.

```c
// ** LED COLOR ASSIGNMENT **
#define COLOR_JAP   LED_GREEN
#define COLOR_USA   LED_RED
#define COLOR_EUR   LED_YELLOW
```

> [!NOTE]
> A **common cathode RGB LED** is required. The default color assignments can be changed at compile time by modifying the definitions above.

### Button Usage

The entire system is controlled using the original **RESET** button. Different press durations activate different functions.

| Button Action | Function |
|--------------|----------|
| **Short Press** (<300ms) | Reset the console |
| **Medium Press** (<1000ms) | Toggle 50Hz / 60Hz (Europe mode only) |
| **Long Press** (≥1000ms) | Select the next region (Japan → USA → Europe) |

> [!TIP]
> Region changes are applied when the RESET button is released. During selection, the RGB LED indicates the currently selected region using the configured color assignment.

[🔝 Back to top](#top)

---

## Building & Flashing

Neptune-SRB is developed using **MPLAB X IDE** and the **XC8 Compiler** for Microchip PIC microcontrollers.

### Development Environment

| Component | Version |
|-----------|---------|
| IDE | MPLAB X IDE |
| Compiler | MPLAB XC8 |
| MCU | PIC16F630 / PIC16F676 |
| Oscillator | Internal 4MHz |
| Programming Interface | ICSP |

### Compiling the Firmware

Open the project in MPLAB X IDE and build it using XC8. The default project configuration is ready for the supported PIC devices and does not require additional modifications.

### Flashing the Firmware

Precompiled HEX files are available from the project's **Releases** page.

Program the firmware using any compatible PIC programmer, such as:

- PICkit 3
- PICkit 4
- MPLAB SNAP

Programming can be performed directly through the ICSP header or by removing the PIC from its socket and programming it externally.

> [!NOTE]
> Existing Neptune Switchless installations can usually be upgraded simply by replacing or reprogramming the original PIC microcontroller.
>
> If In-Game Reset (IGR) is **disabled** (`#define IGR_ENABLE 0`), no hardware modifications are required.
>
> If IGR is **enabled** (`#define IGR_ENABLE 1`), four additional connections to the controller port are required:
>
> | PIC Pin | Controller Port |
> |---------|-----------------|
> | RA5 | Pin 9 (TR) |
> | RA4 | Pin 6 (TL) |
> | RA3 | Pin 1 (UP) |
> | RC1 | Pin 7 (TH) |
>
> Optional **1 kΩ** series resistors may be installed on each controller input for additional protection, although the firmware operates correctly without them. See **Installation Notes** for wiring details.

[🔝 Back to top](#top)

---

## Installation Notes

Neptune-SRB is designed as a firmware replacement for the original **Neptune Switchless Mod** hardware.

### Supported Platforms

- Cosam Neptune
- Sega Genesis / Mega Drive equipped with the Neptune Switchless hardware

### Required Hardware Modification for IGR

In-Game Reset (IGR) requires one additional connection that is not present on the original Switchless installation.

The **RA3** pin of the PIC must be isolated from GND and connected to **Controller Port Pin 1 (UP)**.

<!-- IMAGE HERE: RA3 wiring -->

📷 *Illustration showing the RA3 modification and connection to Controller Port Pin 1.*

> [!IMPORTANT]
> Without this modification, all standard Switchless functions remain fully operational, but In-Game Reset (IGR) will be unavailable.

### Installation Example

<!-- IMAGE HERE: Installed Neptune PCB -->

📷 *Example showing the Neptune Switchless board installed inside the console.*

[🔝 Back to top](#top)

---

## In-Game Reset (IGR)

Neptune-SRB supports software reset directly from the controller, eliminating the need to reach the console's RESET button during gameplay.

The firmware automatically detects the connected controller type and applies the appropriate button combination.

| Controller | Button Combination |
|------------|--------------------|
| Sega Genesis / Mega Drive | **A + B + C + Start** |
| Sega Master System | **Up + B + C** |

The button combination must be held continuously for approximately **1 second** before a reset is triggered. This prevents accidental resets during normal gameplay.

> [!NOTE]
> Controller type detection is performed automatically by monitoring the TH line. No user configuration is required.

### Technical Notes

- Genesis controllers are detected using TH multiplexing.
- Master System controllers are detected when TH remains static.
- The detection method is completely automatic.
- IGR timing is handled by the Timer0 interrupt, providing consistent behavior regardless of game software.

<!-- IMAGE HERE: IGR Timing -->

📷 *Timing diagram showing Genesis and Master System IGR detection.*

[🔝 Back to top](#top)

---

## Configuration

Most Neptune-SRB behavior can be customized by editing the configuration macros near the beginning of the source code.

### Feature Enable

In-Game Reset support can be enabled or disabled at compile time.

```c
// FEATURE ENABLE
#define IGR_ENABLE    1      // 1 = Enable In-Game Reset, 0 = Disable
```

When disabled, the firmware operates as a standard Neptune Switchless replacement without controller-based reset support.

### Region LED Colors

The RGB LED color assigned to each region can be customized by modifying the following definitions.

```c
// ** LED COLOR ASSIGNMENT **
#define COLOR_USA   LED_RED
#define COLOR_JAP   LED_GREEN
#define COLOR_EUR   LED_YELLOW
```

Available LED color definitions are:

- `LED_RED`
- `LED_GREEN`
- `LED_BLUE`
- `LED_YELLOW`
- `LED_CYAN`
- `LED_PURPLE`
- `LED_WHITE`

### User Timing Constants

The following constants control the user interface timing.

```c
// USER TIMING CONSTANTS
#define STD_PRESS   300     // ms
#define EXT_PRESS   1000    // ms
#define IGR_HOLD    1000    // ms
#define RST_PULSE   500     // ms
```

| Constant | Description |
|----------|-------------|
| `STD_PRESS` | Threshold between short and medium button presses. |
| `EXT_PRESS` | Long-press threshold used to enter region selection mode. |
| `IGR_HOLD` | Required controller hold time before an In-Game Reset is triggered. |
| `RST_PULSE` | Duration of the hardware reset pulse. |

### System Timing Constants

The firmware also defines internal timing parameters used for button sampling.

```c
// SYSTEM TIMING CONSTANTS
#define DEBOUNCE_MS 50
#define POLL_MS     100
```

> [!WARNING]
> These values are part of the firmware's internal timing logic. They normally do not require modification and should only be changed if the effects are fully understood.

[🔝 Back to top](#top)

---

## About this Guide

This project is maintained by **Electroanalog VICE** and is released as open-source hardware and firmware.

The goal of Neptune-SRB is to provide a modern, reliable, and fully documented replacement for the original Neptune Switchless Mod while preserving compatibility with existing hardware installations.

If you find this project useful, consider giving the repository a ⭐ on GitHub. It helps other retro gaming enthusiasts discover the project.

### License

This project is distributed under the **GNU General Public License v2.0 or later (GPL-2.0-or-later)**.

You are free to use, modify, and redistribute this firmware under the terms of the GPL.

### Contributing

Bug reports, feature suggestions, and pull requests are always welcome.

If you build your own Neptune-SRB installation, feedback and testing results are greatly appreciated.

---

**Electroanalog VICE**  
*Vintage Integrated Custom Electronics*

© 2026 Electroanalog VICE

[🔝 Back to top](#top)

