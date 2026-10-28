# Phase 4: Rad Pro FM33 port

## Status and next steps (2026-10-27, end of fourth session)
**Rad Pro runs on the unit** (bench 2026-10-25/26). Working on hardware: boot, keys, GM counting (≈9 cpm
background, 0.08–0.09 µSv/h), pulse click + LED, alert LED, all screens, brightness levels, sound/volume menu,
flash saves, power-off fully dark, display sleep, charging icon with USB while on, charging LED held while off
(off on unplug; off with a full battery), USB auto power-on on plug-in. Log: `docs/ut334a-notes.md`.

**On the unit now:** fork `65d937a` (flashed 2026-10-28, image 49d36b88…: auto shutdown + Gray unit on top of `ad932fc`, image 15eec4ab…: serial on UART5 PD0/PD1, RTC invalid-date fix,
`ALERT_WAKES_DISPLAY` on top of `91b25e6`; earlier image
b4674e27…; settings were reset by the full erase): the off-state fix (keys while off after 5 s, USB
re-plug charging screen), the stock-style **Overview** screen (rate large + Average + Cumulative rows; OK cycles
Average/Cumulative → Max + secondary-unit rate → bar), Up/Down = Overview ↔ History, Reset clears
rate/max/average/cumulative, **3 min History tab**, and alerts waking a sleeping display (tone audible). All
bench-checked 2026-10-27, incl. Dusk/Night. Flags `MEASUREMENT_OVERVIEW`, `HISTORY_3_MINUTES`,
`ALERT_WAKES_DISPLAY` (UT334A builds only).

**Build:** `~/.platformio/penv/Scripts/pio run -e uni-t-ut334a-en` in `radpro/platform.io` (in Git Bash `pio` is
not on PATH), then `python <workspace>/radpro/tools/sign.py` from an empty scratch dir →
`uni-t-ut334a/firmware/radpro-uni-t-ut334a-en-3.1.2test2.bin`. Emulator check before flashing, e.g.
`EMU_DEEPSLEEP=real EMU_KEY2=7000:10500:0x400 python tools/fw_emu.py . <bin> <nm.txt> 14000 0 0 0 <png>`
(nm.txt = `arm-none-eabi-nm firmware.elf`; key masks: PA10 OK 0x400, PA15 left 0x8000, PD11 right
0x0800000000000000; `EMU_USB=on:off:on`; `EMU_POKE=tick:sym+off:mask:value` sets settings bits, e.g. theme
`settings+12:0x18:0x08` Dusk / `0x10` Night, primary unit `settings:0xe000:0x4000` cpm / `0x8000` Gy, auto shutdown
`settings+12:0x38000000` (1 h = 0x08000000), its counter `power+16`; SysTick 1 kHz; power on with key mask 0x400
released at tick 1300).

**Flashing procedure** (see `docs/swd-access.md`, user version `docs/install-guide.md`): battery power only
(ST-Link 3.3 V unconnected), `--no-nrst`: `tools/fm33_flash.sh dryrun --no-nrst`, then after "erase it":
`tools/fm33_flash.sh flash <signed.bin> --erase-it --no-nrst` (~6 min). If it reports the controller key-locked,
nothing was erased: battery off 5 s, retry.

**Next:** user day-long test of `65d937a` (2026-10-28), issues reported in a new session; then step 5.

Remaining work (user 2026-10-27: after these two the project is closed). Stop for confirmation after each:
1. **Step 9: done 2026-10-28** (auto shutdown + Gray unit flashed, `65d937a`; Schedule, Records, ICRP page not
   wanted by the user). Original note (from `analysis/ghidra/strings.txt`, see notes 2026-10-27): quick
   ones first, **auto shutdown after N hours** and the **µGy unit label**; then, if the user wants them,
   Schedule (start delay + timed measurement), on-screen Records (10 sessions), ICRP info page. Code + emulator,
   then one flash for all of it (settings reset by the erase: set theme/alarm/time zone again; clock can be set
   over serial `SET deviceTime <unix UTC>` or over SWD).
2. **Step 5, battery calibration (final step):** needs a lower battery. Rad Pro 4.073 V vs meter 4.029 V (+1.1 %)
   at the first point; second point (3.6–3.9 V, USB out, Settings → Statistics → Battery vs meter across the
   battery), then `PWR_BAT_SCALE_FACTOR` (divider 0.5438 → ≈ 0.5497 if it is a pure scale error), flash, close.

Done this session (2026-10-27): flashed `91b25e6` and bench-checked the off-state fix, Overview and 3 min History;
alarm from display sleep fixed (`ALERT_WAKES_DISPLAY`, acknowledge = Left held 1 s); off-state current 3–8 µA
(on ≈ 40 mA, alarm peak ≤ 365 mA); settings survive a battery disconnect, RTC survives short ones (board
capacitance), invalid RTC → 2025-01-01 at power-on (verified); serial moved to **UART5 PD0/PD1 (QFN 30/31)** and
tested with an FTDI on COM7 (TX → 1 kΩ → QFN 30, 115200 8N1, USB charger in). Both repos pushed.
Dropped: step 6 (GM calibration): only an Am-241 smoke-detector source is available (alpha + weak 59.5 keV
gamma, no known dose rate), no reference meter, stock not restored → the "UT334A" preset (68.27 CPM per µSv/h,
from the stock firmware) stays. Step 8 (faster drawing): no slow screen reported.

**Live-debug tools** (firmware running, no flash writes): `tools/fm33_backlight.sh <0..10000>` (ATIM CCR1);
OpenOCD with `fm33-stlink.cfg` + `-c "init; ..."` for register reads/writes (see notes for examples: COLMOD
resend with the core halted, PB2/PC4 pokes). Emulator: `tools/fw_emu.py` (SPI → ST7789 frame PNG, strict FLS
key model, `EMU_DEEPSLEEP=continue`, `EMU_KEY2=start:end:mask`, `EMU_SPI_CMDS_FROM=tick`). Flash-tool model
test: `tools/fm33_fakefls.tcl`.

---

# Plan input (written before the port; kept for reference)

From the original brief (§6, `docs/archive/original-brief.md`), updated with the Phase 1–2 findings.
**First step of Phase 4: check this against the current radpro repo**, then give the user a concrete plan.

## Rad Pro structure (verify against the current repo)
- `platform.io/src/`: portable UI, measurements, settings, datalog. Modify only if necessary.
- `platform.io/src/peripherals/*.h`: the hardware interface the port implements.
- `platform.io/src/stm32/stm32_*.c` (~15 files: adc, buzzer, cmath, comm, display, events, flash, led, power,
  pulsesoundenable, rtc, system, tube, vibrator, voice) on thin helpers `lib/stm32/stm32-ext.h` → needs an FM33 equivalent.
- Per-device files, e.g. `src/stm32/gc03.c/.h`. **GC-03 is the main reference**: 240×320 ST7789 over SPI via
  `lib/mcu-renderer` callbacks (reset, CS, DC, send8, send16).
- `src/sdl/`: simulator backend, a minimal example of the interface.

## Plan
1. `platform.io/src/fm33/fm33_*.c`: system (64 MHz PLL, SysTick, IRQs), events, flash (FM33 key sequence,
   page erase/program), adc (battery), tube (COMP2 IRQ + timestamps), buzzer (GPTIM0 PWM), led (bi-colour),
   power (rails, sleep), rtc (BCD), display (SPI1 + DMA + backlight), keyboard. Comm: see below. Skip voice,
   vibrator, EMF, USB.
2. Device files `ut334a.c/.h`, modelled on gc03, pins from `docs/ut334a-hardware.md`.
3. Register definitions/startup from `vendor/FM33LC0XX_DFP` (do not hand-write register maps); thin
   `fm33-ext.h` in the `stm32-ext.h` style.
4. Build: PlatformIO has no FM33 platform → custom board JSON (Cortex-M0, 256K/24K), linker script, startup,
   `[env:ut334a-fm33lc043n-en]`. Keep all existing envs building.
5. Flashing: pyOCD + FM33LC0XX_DFP pack, or OpenOCD with the DFP flash algorithm (`vendor/FM33LC0XX_DFP/Flash/
   FM33LC04X_FLASH256.FLM`). **Only after the user says "erase it".** Also deliver a restore script for
   `backup/ut334a_stock_1.bin`.
6. Bring-up, one step at a time, each with a test the user verifies on the bench: power rails + clock →
   backlight/display → keys → GM pulse counting (compare CPM with stock on the same background) → speaker →
   battery/charging → RTC → settings/datalog flash → sleep/current draw → comm.
7. Tube sensitivity: start from the stock calibration (below), then match stock readings.

## Porting implications of the findings (see `docs/ut334a-hardware.md`)
- **SWD**: keep PD7/PD8 as SWD (never park them). OpenOCD's `cortex_m` rejects CPUID 0x410CC300; the flashing
  and debug path must handle that (pyOCD with the DFP, or an OpenOCD target tweak). Resolve before step 1 of bring-up.
- **First flash needs the manual NRST hold** (stock firmware blocks SWD). After that, our firmware leaves SWD free.
- **HV supply**: no hardware PWM. PC4 switches the HVbatt rail (VBAT → LM3478 supply + battery divider); PC5
  runs/stops the LM3478 (high = run). Stock firmware: bursts of 1 tick (4.88 ms) in 5 → ~41 Hz, ~20 %, plus a
  burst after each GM pulse. Rad Pro's HV-profile idea (frequency/duty) must be generated in software on PC5
  (no timer AF on PC5). HV ≈ 425 V unloaded (estimate); tube anode resistor 4.7 MΩ.
- **GM input**: PA8 → COMP2 vs VREF/2 (0.6 V), filter on; pulses ≈ 2.1 V, ≈ 60 µs. Use the COMP IRQ +
  a timer timestamp for Rad Pro's pulse timing. Stock has no software dead time.
- **Display**: ST7789 240×320, SPI1 (PB9 SCK, PB11 MOSI), CS PB3, DC PB8, RST PB13, LCD power PB14 (low = on),
  exact init bytes in the hardware map; backlight PWM PB2 (ATIM complementary output CH1N, so MOE must be set; 3.2 kHz, 50 % at stock init).
- **Keys**: left PA15, OK/power PA10, right PD11 (all active low, pull-up). No power switch: "off" = DeepSleep;
  wake on RTC tick + key poll, and on USB (PA9 EXTI).
- **Sound**: PB10 GPTIM0 CH1 → 8002A (always enabled). **LED**: bi-colour between PC2 and PC3.
- **Battery**: PC8 ADC_IN9 vs internal VREF (VREF_CAL at 0x1FFFFB08); divider **0.5438 measured** (stock used
  0.55); only valid while PC4 is on. Charger 4057; status pins not wired to the MCU; USB present = PA9 high.
- **Flash for settings/datalog**: the stock firmware never writes flash, so the layout is entirely ours.
- **Comm**: no free UART pad (PA13/PA14 unrouted). Options: solder to PA13/PA14 (UART0/LPUART0), or no comm.
- **Clocks**: 32.768 kHz crystal on PD9/PD10; stock tick LPTIM 204.8 Hz (its "2 s" window is 1.953 s).
- **Calibration baseline** (stock, C1=C2=1): 66.7 CPM per µSv/h below 0.8 µSv/h, 45.0 between 0.8 and 20,
  39.0 above 20 (≈2.4 % more counts per µSv/h after correcting the stock window error). Tube marking not
  identified ("J442y XRHH 2023"?).
