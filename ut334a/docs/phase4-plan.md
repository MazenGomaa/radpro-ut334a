# Phase 4: Rad Pro FM33 port

## Status and next steps (2026-10-25)
Done in code, verified in the emulator (`tools/fw_emu.py`), not yet on hardware: build env `uni-t-ut334a-<lang>`
(fork `radpro/`, branch `ut334a`), core drivers (clock, SysTick/IWDT, RTC, power-off = DeepSleep), hardware
drivers (display, backlight, keys, tube + HV bursts, buzzer with volume, LED, battery ADC, flash), SWD
flash/restore tools. Details per step: `docs/ut334a-notes.md` (2026-10-25 entries A–D). Comm is a stub.

Next session, in order (stop for confirmation after each):
1. **Dry run** (read-only): wire as in `docs/swd-access.md`, user holds NRST, `tools/fm33_flash.sh dryrun`
   (background, ~3 min window); tell the user to release NRST at ARMED. Expect UNPROTECTED, and note the
   "100 reads" timing (flashing ≈ 6 SWD operations per word: 11 874 words for Rad Pro, 52 833 for the restore).
2. **Build and sign**: `pio run -e uni-t-ut334a-en` in `radpro/platform.io`, then run `radpro/tools/sign.py`
   from a scratch directory; the image is `uni-t-ut334a/firmware/radpro-uni-t-ut334a-en-<version>.bin`
   (128 KB, CRC footer). Never flash the unsigned `firmware.bin` (checksum alarm at every boot).
3. **Flash**, only after the user says "erase it": `tools/fm33_flash.sh flash <signed.bin> --erase-it` with the
   NRST hold. It erases all 512 pages, programs, compares a full readback, then resets. If verify fails:
   `tools/fm33_flash.sh restore --erase-it`.
4. **Bring-up** (one at a time, user checks on the bench):
   a. boots at all? SWD now connects without NRST (PD7/PD8 left free); a dark unit after a reset → restore or debug.
   b. display: image, orientation, colours (MADCTL/INVON), backlight polarity and whether it needs PC4.
   c. keys: left/right/OK, long OK = on/off; power-off → DeepSleep current, wake by key/USB.
   d. GM: counts vs stock on the same background; HV at the tube (HV probe only) with the 5/19 ms bursts.
   e. buzzer clicks and volume levels; LED colours (pulse vs alert).
   f. battery voltage vs a multimeter (divider 0.5438, PC4 settle 5 ms), charging indication on USB.
   g. RTC keeps time through power-off; settings and datalog survive a battery disconnect (flash writes).
   h. sleep current; then calibration (stock ≈ 66.7 CPM/µSv/h low range; tube sensitivity setting).
5. Later: comm over UART0 (PA13 RX / PA14 TX) after the CH340 mod; LCD supply cut in power-off.

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
