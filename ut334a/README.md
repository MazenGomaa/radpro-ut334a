# UNI-T UT334A port: hardware notes and tools

Everything learned about the UNI-T UT334A (FM33LC043N) while porting Rad Pro to it:

| File | Contents |
|---|---|
| `docs/ut334a-hardware.md` | Hardware map: every pin, display init, HV control, GM front end, calibration, battery, clocks |
| `docs/phase4-plan.md` | Port plan and the porting implications of the findings |
| `docs/install-guide.md` | **Start here:** back up your unit, build, flash, restore |
| `docs/swd-access.md` | Connecting an ST-Link (manual NRST hold), protection facts, making a backup |
| `docs/analysis-tools.md` | Ghidra / emulator toolchain used on the stock firmware |
| `docs/ut334a-notes.md` | Chronological log |
| `tools/`, `fm33-stlink.cfg` | OpenOCD config, flash/restore tool (`fm33_flash.sh`), analysis scripts |

The stock firmware is UNI-T's copyrighted code and is **not** included. Paths like `backup/` in these docs refer to
a private workspace. **Before flashing Rad Pro, dump your own unit's stock firmware** (see `docs/install-guide.md`)
so you can restore it if anything goes wrong.

## Changes to shared Rad Pro code

Everything else of the port lives in `platform.io/src/fm33/`. Changes outside it:

| File | Change | Scope |
|---|---|---|
| `src/peripherals/tube.c` | **Upstream bug fix:** `getTubeType()` indexed the tube names with the sensitivity index, so over comm J321 was reported as M4011, HH614 as J321, ..., LND 7317 as SI3BG. It now uses `settings.tubeType`. Not reported upstream. | all devices |
| `src/peripherals/tube.c`, `src/system/settings.h` | "UT334A" tube type: the stock calibration (68.27 CPM per µSv/h), M4011 source factors | `UT334A` |
| `src/system/power.c` | The MCU stays powered while off (`POWER_OFF_SLEEP`): a USB plug-in while off shows the charging screen or, with USB auto power-on, powers on; at boot (= battery connection) auto power-on needs USB present | `POWER_OFF_SLEEP` |
| `src/system/power.c`, `src/system/settings.h` | Low battery as stock: power off below 3.35 V held for 100 s (`PWR_BAT_LOW_VOLTAGE`, `PWR_BAT_LOW_SECONDS`) | `UT334A` |
| `src/main.c`, `src/system/power.h` | `sleepWhilePoweredOff()` in the main loop | `POWER_OFF_SLEEP` |
| `src/peripherals/keyboard.c`, `src/peripherals/keyboard.h` | `isKeyboardIdle()`: SysTick stops in DeepSleep, so sleeping while off waits until the debounced key release has been seen | `POWER_OFF_SLEEP` |
| `platformio.ini`, `tools/platformio.ini.base`, `tools/build-env.py`, `tools/sign.py` | `uni-t-ut334a-*` environments and signed images | – |
| `lib/fm33/`, `scripts/ut334a.ld` | FM33LC0xx headers (CMSIS, register bits) and the linker script | – |
