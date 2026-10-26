# Phase 4: Rad Pro FM33 port

## Status and next steps (2026-10-26, end of session)
**Rad Pro runs on the unit** (bench 2026-10-25/26). Working on hardware: boot, keys, GM counting (≈9 cpm
background, 0.08–0.09 µSv/h), pulse click + LED, alert LED, all screens (after the COLMOD fix), brightness levels
(after the PWM-mode fix), sound/volume menu, flash saves (after the key-lock fix), power-off fully dark with a
clean screen on power-on, display sleep dark (counting continues), wake key only wakes. Log: `docs/ut334a-notes.md`.

**On the unit now:** fork commit `aac7617` (flashed 2026-10-26, image SHA-256 afc69f63…).
**Built, emulator-verified, NOT flashed:** fork commit `0954b16` (= `aa23d2d` + charging screen while off + low battery as
stock; `aa23d2d` added tube preset, BOR, auto power-on, serial: see step 1).
First of them, `7250c07` = `aac7617` + charging indicator hold as stock
(on with USB below 4.1 V, off after 10 min at or above 4.1 V or on USB removal; bench: 4.18 V on the battery
while charging made the old LED go off after one 5 s re-read). Image SHA-256 56e735f7… (rebuild it, below).

**Build:** `~/.platformio/penv/Scripts/pio run -e uni-t-ut334a-en` in `radpro/platform.io` (in Git Bash `pio` is
not on PATH), then `python <workspace>/radpro/tools/sign.py` from an empty scratch dir →
`uni-t-ut334a/firmware/radpro-uni-t-ut334a-en-3.1.2test2.bin`. Check it in the emulator (below) before flashing.

**Flashing procedure now** (see `docs/swd-access.md`, user version `docs/install-guide.md`): battery power only
(ST-Link 3.3 V unconnected: it browns out during erase/program), no NRST needed (`--no-nrst`):
`tools/fm33_flash.sh dryrun --no-nrst`, then after "erase it":
`tools/fm33_flash.sh flash <signed.bin> --erase-it --no-nrst` (~6 min). If it reports the controller key-locked,
nothing was erased: battery off 5 s, retry.

Next session, in order (stop for confirmation after each):
1. Flash fork `0954b16` (after "erase it"; rebuild + sign first): charging LED hold, UT334A tube preset, BOR off in
   DeepSleep with one PLL restart per off period, USB auto power-on, UART0 serial driver, charging screen on USB
   plug-in while off, low battery 3.35 V held 100 s (see the notes, 2026-10-26). Check: USB while off → green LED stays on (≈ 10 min with a nearly full
   battery, longer when lower), off on unplugging; charging icon while on. Double-press left while off shows
   the battery (not checked yet).
2. **Off-state current** with a meter in series with the battery (stock lasted months, so µA range expected).
   Datasheet: ≈ 2 µA typ. DeepSleep with BOR off. If high: PC8/PA8 as stock (open-drain outputs), backlight
   supply path, the 1 s RTC wake-ups (stock does not use them).
3. Settings + datalog survive a battery disconnect (flash writes now work); RTC across power-off / disconnect.
4. Alarm buzzer: user reported it "not right" (details still to get); check alert/pulse volume levels.
5. Battery voltage: Rad Pro 4.073 V vs meter 4.029 V (+1.1 %); take a second point (not while charging), then
   adjust `PWR_BAT_SCALE_FACTOR` (divider 0.5438 → ≈ 0.5497).
6. Off-state charging screen on USB plug-in: fixed offline (fork, see notes); check on the bench (USB in while off
   → charging icon for 5 s, green LED stays on).
7. GM: the "UT334A" preset (68.27 CPM per µSv/h, stock low band) should read as stock at background; compare
   with a reference above 0.8 µSv/h if possible (stock uses 46.1 there); HV at the tube (HV probe only).
8. Optional: faster drawing only if the user finds a screen slow (SPI at 32 MHz is already the per-pixel limit;
   DMA would only overlap rendering with the transfer, via a buffer layer under mcu-renderer). (A fixed 5 s
   averaging option was dropped: user decision 2026-10-26.)
9. Serial: driver written and emulator-tested (UART0, 115200 8N1, opens on USB power). Bench: FTDI on PA13/PA14
   (1 kΩ into PA13, no VCC), USB charger in, `GET deviceId`; then the CH340. Option: HC-05 on UART5 (PD0/PD1)
   with a P-MOSFET supply switch on PC9/PC10 and Rad Pro's DATA_MODE (continuity checks first).
10. USB auto power-on (Settings → Power): plug USB in while off → powers on (setting on); battery reconnect with
   the setting on and no USB → stays off.

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
