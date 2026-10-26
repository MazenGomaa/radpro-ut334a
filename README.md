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

Tested on one unit (2026-10-26):

* Working: boot and power on/off, keys, GM counting, pulse click and LED, alert LED, all screens, brightness
  levels, display sleep, sound and volume settings, settings saved to flash, battery display, charging icon,
  charging LED that stays on until full, USB auto power-on, charging screen when USB is plugged in while off.
* Fixed, not yet tested on the unit: keys and USB re-plug while off once the unit has gone back to sleep.
* Added, not yet tested on the unit: **serial link** for data export and streaming through an added 3.3 V
  USB-serial adapter on the MCU's UART0 (the UT334A has no USB data connection; see the installation guide);
  stock calibration as the "UT334A" tube preset; lower power use while off; low-battery power-off as stock
  (below 3.35 V for 100 s).
* Known issue: the alarm tone is not heard (pulse clicks pause during each alarm flash).
* Not yet verified: battery reading accuracy (reads about 1 % high), clock and saved data across a battery
  disconnect, power consumption while off, counts and calibration against the stock firmware.

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
