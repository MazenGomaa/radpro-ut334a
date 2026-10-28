> **UNI-T UT334A port (unofficial fork).** This fork adds Rad Pro support for the UNI-T UT334A Geiger counter
> (Fudan FM33LC043N). The port was written with Claude (Anthropic's AI assistant) from a reverse-engineered map of
> the hardware, made by decompiling and emulating the stock firmware and confirming the findings on the bench. It is
> experimental: **dump your own unit's stock firmware before flashing** (see the
> [UT334A installation guide](ut334a/docs/install-guide.md)). Not affiliated with UNI-T or with the upstream Rad Pro
> project. Upstream: [Gissio/radpro](https://github.com/Gissio/radpro).

[![Rad Pro Simulator - Try me!](https://gissio.github.io/radpro-simulator/radpro-title-tryme.png)](https://gissio.github.io/radpro-simulator/)

[![License](https://img.shields.io/github/license/Gissio/radpro)](https://github.com/Gissio/radpro/blob/main/LICENSE)
[![Last Commit](https://img.shields.io/github/last-commit/Gissio/radpro)](https://github.com/Gissio/radpro/commits/main/)
[![Build Status](https://github.com/Gissio/radpro/actions/workflows/build-all.yml/badge.svg)](https://github.com/Gissio/radpro/actions/workflows/build-all.yml)
[![GitHub release](https://img.shields.io/github/v/release/Gissio/radpro)](https://github.com/Gissio/radpro/releases)

Try the [Rad Pro simulator](https://gissio.github.io/radpro-simulator/)!

## Introduction

**Rad Pro** is a custom firmware designed to enhance the capabilities of supported Geiger counters. It provides advanced measurement tools, data logging, alerting, and visualization features beyond stock firmware.

On the **UNI-T UT334A**, Rad Pro lets you:

* Monitor radiation levels in real time, with pulse clicks and the pulse LED
* Compute statistically meaningful averages
* Track accumulated radiation dose
* Configure alerts for rate and dose thresholds (alert LED and sound)
* Visualize historical trends over multiple time scales
* Log measurement data to the internal flash, and export it or stream live data over an added serial
  adapter (experimental, see below)
* Generate high-quality random data

### UT334A port status

Tested on one unit (last update 2026-10-28, fork commit `65d937a`):

* Working: boot and power on/off, keys, GM counting, pulse click and LED, alert LED and alarm tone (also from
  display sleep; an alert wakes the display as on the stock firmware), all screens, brightness levels, display
  sleep, sound and volume settings, settings saved to flash, battery display, charging icon, charging LED that
  stays on until full, USB auto power-on, charging screen when USB is plugged in while off, keys and USB re-plug
  while off.
* Stock-style screens: an **overview screen** (instantaneous rate, average rate and cumulative dose together; OK
  switches its lower part to maximum and secondary-unit rate, or the bar), which with History forms the screen
  cycle (Up/Down); a **3 minute history** view.
* Stock features: **µGy units** (Dose units → Gray: the Sv values with a Gy label, as the stock firmware does);
  **auto shutdown** after 1–24 h on battery (Settings → Power, default off; checked in the emulator only).
* **Serial link** for data export and streaming through an added 3.3 V USB-serial adapter on the MCU's UART5
  (PD0/PD1; the UT334A has no USB data connection; see the installation guide). Tested with an FTDI adapter.
* Stock calibration as the "UT334A" tube preset (68.27 CPM per µSv/h, the stock low-range factor); low-battery
  power-off as stock (below 3.35 V for 100 s). Off-state current 3–8 µA. Settings survive a battery
  disconnect; after a long one the clock restarts at 2025-01-01.
* Not yet verified: battery reading accuracy (reads about 1 % high; correction pending), calibration against a
  reference source (above 0.8 µSv/h the stock firmware reads about 1.5× higher than the single preset factor;
  which is closer to the truth is unknown).

Upstream Rad Pro devices export and stream over USB; the UT334A needs the serial adapter for this.

## Installation

Before installing, read the [legal disclaimer](docs/legal.md).

Supported devices:

* [FS2011/YT-203B installation guide](docs/devices/FS2011/install.md)
* [Bosean FS-600/Bosean FS-1000 installation guide](docs/devices/Bosean%20FS-600,%20FS-1000/install.md)
* [Bosean FS-5000 installation guide](docs/devices/Bosean%20FS-5000/install.md)
* [FNIRSI GC-01/JOY-IT JT-RAD01 installation guide](docs/devices/FNIRSI%20GC-01/install.md)
* [FNIRSI GC-03 installation guide](docs/devices/FNIRSI%20GC-03/install.md)
* [GQ GMC-800 installation guide](docs/devices/GQ%20GMC-800/install.md)
* [UNI-T UT334A installation guide](ut334a/docs/install-guide.md) (this fork, experimental)

## Documentation

* [Rad Pro user's manual](docs/users-manual.md) – Easy guide to using Rad Pro
* [Rad Pro reference manual](docs/reference-manual.md) – Technical reference for Rad Pro
* [Ionizing radiation field guide](https://github.com/Gissio/ionizing-radiation-field-guide) – Learn about ionizing radiation
* [Rad Pro developers's manual](docs/developers-manual.md) – Technical details for developers

## Tools & Software

* [GeigerLog](https://sourceforge.net/projects/geigerlog/) – Data analysis software for PC
* [Rad Pro Log Viewer](https://github.com/mayrthom/RadPro-LogViewer) – Data analysis app for Android
* [Rad Pro WiFi Bridge](https://github.com/SunboX/radpro-wifi-bridge) – MQTT WiFi Bridge for Rad Pro

## Acknowledgements

* Rad Pro's user interface leverages the industry-standard [OpenBridge 6.1](https://www.openbridge.no/) design system.
* Special thanks to `Alfred90`, `danielhahn76`, `felixsteghofer`, `DG0MG`, `murkle`, `cromagn`, `ivohar`,`mryndzionek`, `JantarX`,  `mizsha`, `Tig3rch3n`, `Arparela`, `w01fdev`, `martin-machacek`, `dsl71`, `Davegsm82`, `alfmck`, `jonwhite227`, `dc1rdb`, `ihrapsa`, `Montecri`, `boromix`, `maxobe`, `TheAsgard`, `ugopapetti`, `NuclearPhoenixx`, `technils`, `Ilja-code-art`, `ICantMakeThings`, `mayrthom`, `hackra76`, `plamenbe`, `bwisn`, `CalmWookie`, `guestus`, `Marfibooy`, `fonix232`, `iROOT`, `starchaser01`, `ErrorTPkR`, `1n5aN1aC`, `jdogg836`, `ge0rg` and many others for their invaluable testing and feedback.
* Special thanks to `pixitha` for FS2011 (GD32F150) board photos with a J305 Geiger tube, `felixsteghofer` for FS2011 (GD32F103) board photos, `DG0MG` for Bosean FS-600 board photos, `murkle` for Bosean FS-1000 board photos, `ivohar` for FNIRSCI GC-01 schematics, `oOblik` for Bosean FS-5000 board photos, and `guener` for FNIRSI GC-03 board photos.
