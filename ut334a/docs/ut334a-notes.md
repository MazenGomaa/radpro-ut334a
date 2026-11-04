# UT334A Rad Pro port: log

Facts live in `ut334a-hardware.md` (hardware), `swd-access.md` (debug access), `analysis-tools.md` (tools),
`phase4-plan.md` (next steps). This file records what happened and why decisions were made.

## 2026-10-25: Phase 1, SWD connection and backup
- OpenOCD from PlatformIO (xPack 0.12.0+dev); the brief's `hla_swd` config failed, switched to `transport select swd`.
- Hours of "init mode failed": partly bad wires (fixed), but mainly because the stock firmware parks PD7/PD8.
  The datasheet (`datasheets/FM33LC0xx_datasheet_V1.9.pdf`) confirmed pads 2/3 = QFN 1 (PD8/SWDIO) / 32 (PD7/SWCLK),
  pad 1 = QFN 2 (NRST).
- SWD answered only while NRST was held low by hand (the clone's RST pin does not work on this board).
- OpenOCD `cortex_m` rejected CPUID 0x410CC300 → switched to a `mem_ap` target.
- Halt-on-reset via DHCSR/DEMCR, then: option bytes `AA 33 55 CC` → **unprotected**, ACLOCK off.
  Flash dumped ×3 (identical SHA-256), plus info area and RAM, into `backup/`.
- Mistake made and fixed: experiment scripts left an FPB breakpoint and halting debug enabled after the link died;
  since those survive NRST, the unit stayed dark until `tools/fm33_clear_debug.tcl` cleared them under NRST hold.
- `fm33-stlink.cfg` rewritten (swd transport, mem_ap, halt-on-reset procs, still no flash bank).

## 2026-10-25: why SWD dies under the stock firmware (resolved)
- Located live with FPB breakpoints (`tools/swd_bisect.tcl`): SWD dies inside board GPIO init 0x6B64, which sets
  PD0/PD1/PD7/PD8 as push-pull outputs driven low. A whole-image search shows PD7/PD8 are never used again →
  parked "unused" pins, no hardware function. DeepSleep (power-off only) is not the cause.
- Open curiosity: pad 2 read 0 V with NRST held, despite the datasheet's ~100 kΩ SWDIO pull-up in reset.

## 2026-10-25: Phase 2b, static analysis
- Installed: FMSH FM33LC0XX_DFP 3.0.4 (`vendor/`; from FMSH's own server, linked on keil.arm.com; the keilpack
  mirror returned BlobNotFound), Ghidra 12.2 + JDK 25, Unicorn. See `analysis-tools.md`.
- Soft-float helpers identified by emulation; dose formula, calibration ranges, defaults (C1=C2=1.0), timing
  (LPTIM 204.8 Hz, window really 1.953 s) and all peripheral configs derived by emulating the firmware's own code.
- Finding: the stock firmware never writes flash; settings and the 10 records live in RAM (DeepSleep retention).

## 2026-10-25: bench session (user)
- Keys: left PA15 (QFN 5), OK/power PA10 (QFN 8), right PD11 (QFN 29).
- HV: PC5 (QFN 20) → 200 Ω → N-FET → 160 kΩ → LM3478 FA/SD (47 kΩ pull-up to HVbatt): PC5 high = converter runs.
  PC4 (QFN 19) → N-FET → P-FET: VBAT → HVbatt. PB14 (QFN 16) → P-FET → LCD supply.
- PC2/PC3: anti-parallel bi-colour LED. 8002A SD → GND; input ← PB10 via coupling cap + 20 kΩ.
- PA9 (QFN 7): USB 5 V divider (series resistor marked "560"), same net as the 4057 charger VIN. PA13/PA14: no tracks.
- Battery divider: 2.1970 V out at 4.040 V battery → 0.5438 (stock firmware assumes 0.55).
- HV: 314 V at the multiplier, 233 V at the tube after the 4.7 MΩ anode resistor (multimeter) → ≈ 425 V
  open-circuit behind ≈ 3.5 MΩ, assuming a 10 MΩ meter (estimate).
- "Dead tube" scare: a solder bridge QFN 5–6 (PA15 key ↔ PA8 GM input) held PA8 at ~0.5 V, under the 0.6 V
  threshold. After removing it: pulses ≈ 2.1 V peak, ≈ 60 µs decay; the stock firmware counts normally.
- Lesson: QFN-32 is fine-pitch; probe with a sharp tip and re-inspect for bridges afterwards.

## Decisions
- Analysis only via backup + emulation; no live register snapshots (stock firmware blocks SWD).
- Rad Pro must keep PD7/PD8 as SWD.
- Next: Phase 4 (`phase4-plan.md`). Flashing requires the user's explicit "erase it".

## 2026-10-25: Phase 4 start, Rad Pro repo check
- Cloned github.com/Gissio/radpro into `radpro/` (shallow, commit c7454800, "3.1.2test2", 2026-07-28).
- Structure matches `phase4-plan.md`: `platform.io/src/{peripherals,stm32,sdl,system,ui,measurements}`,
  `lib/stm32/stm32-ext.h` (2213 lines), `lib/mcu-renderer` (ST7789), per-device `src/stm32/gc03.c/.h`.
- Build: `[base]` uses `platform = ststm32`, `framework = cmsis`, qfplib soft-float via `--defsym`; devices are
  selected by `-D<DEVICE>` + feature flags (`DISPLAY_240X320`, `DISPLAY_COLOR`, `TUBE_HV_PWM`, `BUZZER`, ...);
  board JSONs carry `FLASH_BASE_/FLASH_SIZE_/FLASH_PAGE_SIZE`. GC-01 (240×320 colour) runs in 20 KB RAM → 24 KB is fine.
- `TUBE_HV_PWM` = the HV-profile menu (frequency/duty on a timer pin). The UT334A's PC5 is a run/stop gate for the
  LM3478, not a switching signal, so this flag probably does not fit (decision pending).
- PlatformIO present (`~/.platformio/penv/Scripts/pio.exe`, gcc-arm toolchain installed).

## 2026-10-25: Phase 4 decisions (user)
- HV: no `TUBE_HV_PWM` / HV-profile menu; PC5 gated in software like stock (~41 Hz, ~20 %, burst after each pulse).
- Comm: user will wire PA13/PA14 to a CH340 fed from the on-board USB-C. Datasheet: PA13 = UART0_RX,
  PA14 = UART0_TX (also LPUART0); only PB12 is 5 V tolerant, VDD is 3.0 V → CH340 I/O must run at 3.3 V.
  Without `DATA_MODE`, Rad Pro opens comm while USB power is present (PA9), which fits a VBUS-powered CH340.
- Repo: own repo for the port (layout/visibility pending).
- Repos: public fork github.com/MazenGomaa/radpro-ut334a (branch `ut334a`, default branch; port code + `ut334a/`
  with copies of docs/tools via `tools/sync_to_fork.sh`); private github.com/MazenGomaa/ut334a-research (this
  workspace: backup, analysis, vendor, datasheets). The stock firmware is not published (UNI-T copyright); the fork
  tells users to dump their own unit first. Workspace `docs/`/`tools/` are the master copies.
- Features: drop chess game, vibrator, voice, EMF, HV profiles, pulse-sound enable, removable battery, bootloader,
  USB auto power-on, data mode. Keep everything else; pulse/alert share the bi-colour LED. English first.
- USB-C D+/D- are not connected on the board; the user will jumper them to the CH340.
- Clone question: a web search found no source naming another maker's device as the UT334A's origin (open).

## 2026-10-25: Phase 4 step A, build skeleton (done)
- Backup guide for users: `docs/backup-guide.md` (stock firmware stays out of the public fork, user decision).
- Env `uni-t-ut334a-en` (all languages generated): section `[uni-t-ut334a]` in `tools/platformio.ini.base` +
  `tools/build-env.py` (regenerated `platformio.ini`, additions only). `platform = ststm32` for the toolchain,
  **no framework** (framework-cmsis would pull an STM32 startup); board `boards/fm33lc043n.json`
  (`-D FM33`, flash 0x0/256K, page 0x200, word 4; datasheet: 512 B page erase, 32-bit programming).
- Flags: UT334A, PWR_USB, KEYBOARD_3_KEYS (same layout as FS5000: left, right, OK = power), DISPLAY_240X320,
  DISPLAY_COLOR, BUZZER, PULSE_LED, ALERT_LED, LED_MULTIPLEX. FIRMWARE_SIZE 0x20000 for every language, so the
  settings/datalog layout does not move between languages; `scripts/ut334a.ld` caps the code at 128 KB.
- `lib/fm33/`: DFP device headers + CMSIS-Core 6 headers (unmodified, see its README), `fm33.h`, `fm33-ext.h`.
- `src/fm33/`: GCC startup (the DFP startup is Keil-only), `device.h`, `ut334a.h` (pins from the hardware map),
  stub drivers (no hardware access; flash stubs never write), `ut334a.c`. Only shared-code change:
  `TUBE_TYPE_DEFAULT` for UT334A in `settings.h` (M4011 for now; calibration at bring-up, stock ≈ 66.7 CPM/µSv/h).
- Result: links; RAM 4.1 KB / 24 KB, code 39.8 KB (stubs). Vector table: SP 0x20006000, reset vector Thumb.
  `fnirsi-gc03-en` and `fs2011-stm32f051c8-en` still build.
- Datasheet: IWDT is off after power-on reset (so no reset loop before the watchdog code exists).

## 2026-10-25: Phase 4 step B, core drivers (done, emulator-verified; not on hardware yet)
- `tools/svd_header.py` → `lib/fm33/fm33lc0xx_bits.h` (`<TYPE>_<REG>_<FIELD>_Pos/_Msk`), cross-checking every
  DFP header member against the SVD. Disagreements: **VREF block** (SVD 0x0C too high; the datasheet and the stock
  firmware's accesses at 0x4001A80C/0x4001A810 confirm the header), UART4/5 TODR and some USB regs (unused).
  Naming aliases: timer `CCMRx` = SVD `CCMRx_Output`; `COMP1/COMP2->CR` = SVD `COMP_CR1/CR2`.
- Clock (`fm33_system.c`): FMSH SystemInit equivalent (trims from 0x1FFFFB20/40/44, PDR/BOR, DBG_CR=3, SWD
  pull-ups), XTLF on PD9/PD10, PLL 8 MHz/8 × 64 = 64 MHz, FLS_RDCR.WAIT=2. Emulating both firmwares gives the
  **same register writes** as stock 0xC6FC/0xD064/0x6DD8(64) (PLLCR 0x003F0030, SYSCLKCR 0x0A000002).
- Events: SysTick 1 kHz (LOAD 63999), IWDT 4 s. **Option bytes IWDTSLP = 0xF (not 0xA) → the IWDT cannot be frozen
  in Sleep/DeepSleep** (datasheet 9.5), which is why the stock firmware wakes every second; we do the same.
- Power "off" = DeepSleep (no latch): `setPowerEnabled(false)` only allows `sleepWhilePoweredOff()`, a new
  main-loop hook behind `-DPOWER_OFF_SLEEP` (`main.c`, `power.h`). It cannot live in `sleep()`/`syncTick()`:
  Rad Pro calls `syncTick()` everywhere as "wait for the next tick" and those must return (first attempt hung there,
  found by emulation). DeepSleep: PMU_CR = SLPDP | PMOD=2 (0x202, same as stock), SLEEPDEEP clear, WFI; wakes on
  RCHF 8 MHz → PLL restarted. Wake sources: keys = WKUP0 (PA15), WKUP1 (PA10), WKUP6 (PD11), falling edge,
  WKUP_IRQn 22; USB = PA9 on EXTI2 (ASEL 01, rising, filter; EXTI clock from LSCLK); RTC 1 s (SEC_IE).
- RTC (`fm33_rtc.c`): BCD calendar, double read, WER = 0xACACACAC (moved here from step C: needed for the
  watchdog wake-up). Reset flags: RCC_RSTFR, write 1 to clear.
- `initGPIO` drives a safe state first: PC5 (HV run) low, PC4 (HVbatt) low, PB14 high (LCD off), LED/speaker low.
- `tools/fw_emu.py`: runs our build from reset with simulated SysTick. Boot without a key: battery screen 5 s →
  DeepSleep at tick 5003; wake → PLL restart → re-sleep; a held key prevents DeepSleep.
- **Not flashable yet**: the stub display leaves the renderer callbacks NULL (would hard-fault). Step C first.

## 2026-10-25: Phase 4 step C, hardware drivers (done except comm; emulator-verified, not on hardware)
All register values copied from emulating the stock routines (`tools/stock_trace.py`) or the FMSH flash algorithm.
- Display (`ut334a.c`): SPI1 master TX-only, BAUD 0 = 32 MHz, 8-bit (stock 0xBA90); PB9/PB11 digital function,
  DFS 0. mcu-renderer ST7789 (reset, SLPOUT, COLMOD, MADCTL 0) + stock panel bytes (0x5960) incl. INVON. LCD supply
  PB14 low before init. Text buffer 88×88 (RAM 11.9 KB total). No DMA (GC-03 does without).
- Backlight (`fm33_display.c`): ATIM PSC 1/ARR 9999 (3.2 kHz), OC1M 111, CH1N only, MOE; PB2 DFS2 = ATIM_CH1N;
  Rad Pro's gamma table on CCR1. **To check on the bench: polarity, and whether the backlight needs PC4 (stock
  turns PC4 on before the LCD).**
- Keys: FS5000-style 3-key hooks.
- Tube (`fm33_tube.c`): COMP2 INP1 (PA8) vs VREF/2, CMP2DF, rising-edge IRQ (CMP2SEL 01), priority 0; BSTIM32
  free-running 1 MHz for Rad Pro's pulse timing. HV from SysTick: PC5 5 ms on / 19 ms off (24 ms) + 3 ms after
  every pulse (stock: until its next 4.9 ms tick). `setTubeHVEnabled` also switches PC4 and the comparator.
  `readTubeDet` = CMP2O (stuck high = shorted tube).
- Buzzer: GPTIM0 PSC 2/ARR 13399 (1.592 kHz, stock), PWM mode 1, PB10 DFS10 = GPTIM0_CH1; **`-DBUZZER_VOLUME`
  enabled** (Rad Pro volume menu; the stock firmware also sets volume by duty).
- LED: plain GPIO on PC3 (pulse) / PC2 (alert); LED_MULTIPLEX keeps them from being on together.
- Battery ADC (`fm33_adc.c`): stock FL_ADC setup (RCHF/8 = 1 MHz, OVSEN 16×, SMTR 0xB77), IN9 vs VREF channel via
  VREF_BUFCR, VREF_CAL 0x1FFFFB08 (complement-checked), divider 0.5438. PC4 switched on for 5 ms if off (settle time
  to check). Charging = USB && < 4.1 V, as stock.
- Flash (`fm33_flash.c`): vendor FLM (disassembled): FLASH_PCE + OPCCR2.FLASHCKE, page erase (ERTYPE 00, keys
  0x96969696/0xEAEAEAEA, 0x1234ABCD into the page, ERD), program (keys 0xA5A5A5A5/0xF1F1F1F1, 32-bit, PRD), KEY = 0
  after each op. FLM DevDscr confirms **512-byte erase units**. Refuses anything below STATES_BASE (0x20000).
  `verifyFlash`: software CRC-32/MPEG-2 (nibble table), checked equal to `tools/sign.py` on random data;
  env added to `sign.py`. Flash images to use are the signed ones (`sign.py` output).
- Emulator (`tools/fw_emu.py`) now models FLS/SPI/ADC and can hold/release keys and inject pulses. Results:
  OK held 1.3 s → splash → measuring: PC4 on, COMP2 on, PC5 5/19 ms; pulses counted, click + LED, burst extended;
  unsigned image → checksum alarm, signed → none; first boot erases/writes the settings and datalog pages.
- Sizes: en 52 KB, zh_CN 77 KB (slot 128 KB); RAM 11.9 KB / 24 KB. GC-03, FS5000, FS2011 still build.
- Left for later: comm (UART0 on PA13/PA14, after the CH340 mod); LCD supply cut in power-off (now SLPIN only).

## 2026-10-25: Phase 4 step D, flashing tools (written and tested offline; dry run pending)
- pyOCD not installed and would need connect-under-reset (the ST-Link RST does not work here), so flashing uses
  the proven OpenOCD path: manual NRST hold → `fm33_arm` → halt at reset vector → flash controller over SWD
  (datasheet 7.4.3: SWD may page-erase and program).
- `tools/fm33_flash.tcl`: procs only (loaded after the unchanged read-only `fm33-stlink.cfg`): FLM Init clocks,
  page erase, word program, error flags checked, refuses addresses outside 0x0–0x3FFFF and unaligned pages,
  `fm33_reset_run` = clear debug + SYSRESETREQ (the halted PC belongs to the old image).
- `tools/fm33_flashgen.py`: image → erase all 512 pages one by one (stock data reaches 0x3BA27, 478 pages, so the
  settings/datalog area must be cleared too) + program non-0xFF words; checks the vector table.
- `tools/fm33_flash.sh dryrun | flash <signed.bin> --erase-it | restore --erase-it`: retry loop for the NRST hold,
  protection must read UNPROTECTED, restore checks the backup SHA-256 first, full 256 KB readback compared with
  the expected image. Rad Pro = 11 874 words, stock restore = 52 833 words.
- Offline test: the procs run inside OpenOCD against a fake flash controller produce exactly the expected image
  for both Rad Pro and the stock backup; guards reject 0x40000, 0x1FFFFC00 and unaligned erases.

## 2026-10-25: end of session
- State saved for the bench session: code complete except comm, everything emulator-verified only. Next steps
  are listed at the top of `phase4-plan.md`; `CLAUDE.md` points there.

## 2026-10-25: bench step 1, dry run (read-only) OK
- `tools/fm33_flash.sh dryrun`: DPIDR 0x0BB11477, armed, halted at reset vector, option bytes `AA 33 55 CC`,
  ACLOCK 0xFFFFFFFF ×2 → UNPROTECTED (unchanged). UID as before. FLS_EPCR/FLS_ISR 0, RCC_PCLKCR2 0x80. Core released.
- SWD 125 kHz (200 kHz not available): 100 reads = 368 ms → 3.7 ms per operation. Estimate at ~6 ops/word:
  Rad Pro ≈ 71 k ops ≈ 4.5 min, stock restore ≈ 317 k ops ≈ 20 min (within the script's 30 min timeout).

## 2026-10-25: bench step 2, build and sign OK
- Clean build `uni-t-ut334a-en` (fork commit 2b93cba): RAM 11 940 B, flash 47 292 B. Note: in Git Bash `pio` is
  not on PATH; use `~/.platformio/penv/Scripts/pio`.
- Signed from a scratch dir: `radpro-uni-t-ut334a-en-3.1.2test2.bin`, 131 072 B,
  SHA-256 `838e1615bb021023…837b`. Vectors: SP 0x20006000, Reset 0x000053A5; 0x20 = STATES_BASE 0x20000,
  0x24 = 0x200; CRC footer @0x1FFFC = 0x32C5D2DA.
- `fm33_flashgen.py`: 11 874 words, vector check passed; expected image = the signed image + 128 KB of 0xFF.

## 2026-10-25: bench step 3, first flash attempt FAILED (tool bug), fixed offline
- User said "erase it". `fm33_flash.sh flash`: all 512 pages erased OK, then program failed at 0x4 with
  FLS_ISR 0x00080200 = KEYERR, KEYSTA 100 (key error lock, cleared only by reset). Only word 0 was programmed.
- Cause: `fm33_program_words` set PREQ once before the loop. The controller clears PREQ after every program, so
  the second key pair arrived without PREQ → key error. The vendor FLM ProgramPage (disassembled) clears EREQ and
  sets PREQ before every word; the port driver `fm33_flash.c` programWord already does. Erase was fine because
  `fm33_erase_page` sets EREQ every page.
- Fix: EPCR (PREQ) written before every word. New strict model `tools/fm33_fakefls.tcl` (clears the request
  after each op, locks on a key without the matching request) reproduces the bench failure exactly with the old
  proc; with the fix Rad Pro and the stock restore both read back identical (87 k / 375 k SWD ops ≈ 5.5 / 23 min).
- Unit state now: main flash blank except word 0, option bytes/info area untouched (page erase only), so it
  cannot boot (dark) but SWD is free.

## 2026-10-25: bench step 3, second flash attempt cut off: brown-outs on the ST-Link 3.3 V
- User said "erase it" again. Erase OK; programming died with an SWD fault on FLS_KEY. Diagnostics (read-only):
  a first dump read every programmed word as 0 (garbage, taken across a reset); DHCSR showed S_RESET_ST and
  retired instructions mid-session; a repeated dump showed the real state: **words 0..0x1BA0 (1769) correct,
  rest erased**. IWDT ruled out (IWDT_CR 0, DBG_CR 3, no IWDTN flag). 8-bit reads of the option bytes glitched
  during a reset (32-bit reads always `0xCC5533AA 0x0000FFFF`, unchanged).
- Cause (user): **the battery was disconnected and the board ran from the ST-Link 3.3 V pin**; it browns out under
  erase/program current (RSTFR had PDRN). Rule 6 stands: battery only, ST-Link 3.3 V unconnected.
- With the stock firmware gone, SWD connects **without NRST** (no resets in 60 s hands-off). Added `--no-nrst` to
  `fm33_flash.sh` (vector catch + SYSRESETREQ, `fm33_reset_halt` in `fm33_flash.tcl`): no tweezers on pad 1.

## 2026-10-25: bench step 3 done, Rad Pro flashed and verified
- On battery, ST-Link 3.3 V unconnected, `--no-nrst`: dry run OK (SYSRESETREQ halt, unprotected, 100 reads 340 ms),
  then `fm33_flash.sh flash radpro-uni-t-ut334a-en-3.1.2test2.bin --erase-it --no-nrst`: 512 pages erased,
  11 874 words programmed, full 256 KB readback identical to the expected image, SYSRESETREQ issued.
  Next: bring-up 4a (does it boot).

## 2026-10-25: bring-up 4a, first boot of Rad Pro on the unit
- Boots (long OK). Works: pulse click, pulse LED, red alarm LED, keys change the display. Alarm flashes the
  screen (Rad Pro alarm signalling). Alarm buzzer "not right" (details pending).
- Display: portrait layout recognisable (title bar, readout, units) but with striped/speckled bands and doubled
  text. Emulator (fw_emu.py now decodes SPI1 + CS/DC as ST7789 → PNG) renders the same build clean (Day theme,
  correct orientation), so the byte stream logic is right.
- Live SWD checks with the firmware running (no reset): SPI1 CR1 0x100, CR2 0x9 (TXO, SPIEN), ISR 0x1002 →
  no TXCOL (no dropped bytes). Live CR1 BAUD → ÷16 (4 MHz): same picture → not signal integrity or a TXBE/BUSY
  timing race. Next: sharp photos of main screen + menu to compare with the emulator frames.

## 2026-10-25: display garbage solved (COLMOD ignored in sleep)
- Upright photos: every text line repeated 3x side by side, fine stripes in greys, colour speckles → panel in
  18-bit mode (ST7789 default COLMOD 0x66) while we send RGB565. mcu-renderer sends COLMOD right after reset,
  before SLPOUT; this panel ignores it there. Stock: SLPOUT, 120 ms, MADCTL, COLMOD 0x05.
- Live test (core halted, `3A 05` sent over SPI1 from SWD, resumed): all screens clean (menu, readouts,
  history). Survives Rad Pro power off/on (panel kept powered).
- Fix in `ut334a.c` displayInitSequence: SLPOUT, 120 ms, MADCTL 00, COLMOD 05 first. Emulator frame identical.
- Also confirmed: long press left in a measurement view = Rad Pro EVENT_KEY_RESET (clears that measurement).
- SPI1 restored to ÷2 (32 MHz) live after the 4 MHz test.

## 2026-10-25: backlight polarity (bench, live CCR1 writes with tools/fm33_backlight.sh)
- With OC1M = PWM mode 2 on CH1N: CCR1 0 brightest, dimmer step by step, 10000 dark → the backlight is lit
  while PB2 is low. So Rad Pro "high" (CCR1 = 10000) was a black screen, not a crash (core kept running, no
  reset flags; silence = 9 cpm background). Also explains the backlight lit while off/charging (off = PB2 low).
- Fix: PWM mode 1 (on-time = CCR1), DISPLAY_BACKLIGHT_ACTIVE_LOW (off = PB2 high). Emulator: PB2 set at init,
  CCMR1 0x68, CCR1 0x14BE.
- Battery: Rad Pro 4.073 V vs meter 4.029 V (+1.1 %), divider tweak pending a second point.
- Volume menu is Settings → Sound (not Pulses).

## 2026-10-25: flash key lock (third flash attempt refused at page 0; nothing erased)
- `fm33_flash.sh flash` stopped at the first page erase: FLS_ISR 0x00080200 (KEYERR, KEYSTA 100) already set
  before the session. Flash contents verified intact (first 128 KB = the running build).
- Bench test after a power-on reset, core halted, clocks on (FLASH_PCE + FLASHCKE): FLS_ISR 0 → one write
  KEY = 0 → 0x00080200. **KEY = 0 in the protected state is a key error**; only one KEY write right after an
  operation is allowed (closes it). The lock survives SYSRESETREQ; power-on reset (battery) or NRST clears it.
  With the flash clocks off the status reads 0 even when locked.
- Both our tool (`fm33_flash_end`) and the port driver (`lockFlash`) wrote KEY = 0 a second time. So the good
  flash ended locked, and Rad Pro locked the controller at its first save: **settings were never stored**.
- Fixes: no extra KEY write in `fm33_flash_end` / `lockFlash`; `fm33_flash_begin` refuses a locked controller
  ("disconnect the battery, nothing was erased"). Models: `fm33_fakefls.tcl` and `fw_emu.py` now lock on a stray
  KEY = 0 (both reproduce the bench lock with the old code); fixed tool ends unlocked for Rad Pro and the stock
  restore; fixed firmware does its 4 first-boot flash operations without a lock.
- Flashed after a battery power cycle: build with display init + backlight + flash-lock fixes (sign4,
  e235c591…), 11 873 words, VERIFY OK, controller left unlocked. After Rad Pro's first boot (it ran its flash
  operations: FLASH_PCE set) FLS_ISR reads 0 → no lock.

## 2026-10-26: backlight off level corrected; stock-style power-off parking; charging LED
- User: power-off and display sleep kept the backlight lit (not so with the earlier builds). Live, Rad Pro off:
  PB2 high (our "off") = lit; PB2 driven low = dark. So the backlight is lit while PB2 is HIGH; the inverted
  sweep came from the PWM mode alone (with only CC1NE, CH1N follows OC1REF). DISPLAY_BACKLIGHT_ACTIVE_LOW
  removed, PWM mode 1 kept (brightness levels confirmed OK by the user).
- Power-off now as stock 0xA884 for the display: before DeepSleep `parkDisplay()` drives PB9/PB11 (as GPIO),
  PB13, PB3, PB8 low and cuts the LCD supply (PB14 high). The next SPI transfer powers it up again: supply on,
  RESX pulse, 120 ms, full init (SLPOUT, MADCTL, COLMOD 05, …). PC4 (HVbatt) was already low when off.
- Charging indicator while off, as stock: PC3 (colour A) on while USB powered and battery < 4.1 V, battery
  re-read every 5 s.
- Emulator (now able to run through DeepSleep, EMU_DEEPSLEEP / EMU_KEY2 / EMU_SPI_CMDS_FROM): boot → off →
  parked + DeepSleep at 5.4 s → OK held at 7 s → supply on + reset at 8.0 s, SLPOUT 120 ms later, clean frame.
- Still to measure: off-state current with a meter in series with the battery (stock lasted months).

## 2026-10-26: fork aac7617 flashed
- Rebuilt `uni-t-ut334a-en` at fork `aac7617`, signed (image SHA-256 afc69f63…). Emulator: boot → off →
  DeepSleep → OK held at 7 s → wake, 4 flash operations, 0 key-error locks, clean frame after wake.
- Dryrun `--no-nrst`: unprotected, FLS_ISR 0. Flash after "erase it": 512 pages erased, 11 955 words,
  VERIFY OK (256 KB readback b2e3d576…), reset and run. Bench checks of step 1 to follow.
- Power-off: backlight fully dark (user).
- Power-on after parking: clean screen, normal backlight (user).
- Display sleep: backlight fully dark, pulse click + LED continue (user).
- Wake from display sleep (left key): clean screen, normal brightness (user).
- Wake press only wakes the screen (no view change) (user).
- USB plugged while off: screen stays dark; LED (colour A = **green**) lights a couple of seconds, then off.
  Expected steady while battery < 4.1 V. Suspect: with charge current flowing the battery terminal voltage rises
  above the 4.1 V threshold (4057 CV 4.2 V; Rad Pro also reads +1.1 % high), so the first read is under 4.1 V and
  the next 5 s re-read is over. To confirm with a meter on the battery while charging.
- Meter, off + USB: battery 4.18 V (> 4.1 V threshold) → cause confirmed. User: stock kept the green LED on
  while charging, until full.
- Stock 0x26F4 has a hold-off our port lacks: battery (8-sample avg) < 4100 mV and USB → charging flag on,
  PC3 set, counter 0x20000E68 = 0; otherwise the flag clears only once the counter passes 300 (or USB gone).
  The counter is incremented in `measurement_tick` (capped at 350), most likely once per 1.95 s window →
  ≈ 10 min of ≥ 4.1 V before "Battery Full" (per-step instead would be ≈ 30 s; nesting not fully certain).
  Our `isBatteryCharging()` drops as soon as one 5 s re-read is ≥ 4.1 V.
- Fix (fork `7250c07`): `isBatteryCharging()` now latches as stock: on with USB below 4.1 V, off when USB
  is removed or after `PWR_BAT_CHARGING_HOLD_TICKS` (600 s) of readings at or above 4.1 V. Used by the
  off-state LED and Rad Pro's charging icon. No DeepSleep while on USB, so SysTick times the hold.
- `fw_emu.py`: `EMU_USB=tick` (PA9 high), `EMU_BAT=tick:raw` (IN9 step; 2640 ≈ 3.9 V, 2830 ≈ 4.18 V).
  Test build with a 20 s hold, USB from 0, 4.18 V from 15 s: PC3 set at 0.37 s, cleared at 35.0 s. Release build
  (600 s, image SHA-256 56e735f7…): boot → off → wake run clean, 0 key-error locks. Not flashed.

## 2026-10-26: stock features not yet in the port; UT334A tube preset; power-off parking (offline)
- Review of the stock image against the port. Gaps: (1) calibration, (2) part of the power-off parking,
  (3) display DMA (optional). Replaced on purpose by Rad Pro: rate algorithm, low-battery cut-off (stock
  3.35 V, Rad Pro 3.0 V), gauge, hidden C1/C2 menu, alarm patterns, timers, Chinese font.
- **Tube preset "UT334A"** (user decision: use the stock calibration), new default tube type for the UT334A
  builds: 68.27 CPM per µSv/h = stock low band (k = 0.9 → 66.7) corrected for the 1.953 s window counted as
  2 s. A single value: stock reads 1.48× higher from 0.8 to 20 µSv/h (46.1) and 1.71× above (39.9). Source
  factors copied from M4011 (tube model unknown). Was M4011 (108.3) → background readings rise ×1.59
  (9 CPM ≈ 0.13 µSv/h, as stock). Flashing erases the settings page, so the new default applies.
  Also fixed upstream `getTubeType()` (indexed the names by the sensitivity index: LND 7317 reported as SI3BG).
- Stock `0xA884` decoded (register-level): see the hardware map, §7. Versus the port: COMP2 (tube off), ADC
  (off after each conversion), LPTIM/DMA/SVD (unused) already match. PC8/PA8: stock open-drain outputs, port
  analog (input buffer off): no current difference expected, left as is. VREF: DeepSleep turns it off.
- Port changes: BOR off during DeepSleep as stock (BORCR |= OFF_BOR_1P2, restored after wake; PDR stays on).
  PLL restarted once on leaving the off loop instead of after every RTC 1 s wake (port-only overhead; stock
  does not wake every second). Emulator: 1633 sleep entries 5.37–7.0 s, BORCR 0x0F/0x0E around each, PLL
  restarted only at 7.0 s (OK held), clean frame after wake, 0 key-error locks. Not flashed; off-state current
  still to measure (bench step 2). Fork commit `ca3f85a`.
- Stock has no energy (source) correction: only the three rate bands and C1/C2. With Rad Pro's default source
  (Cs-137, factor 1.0) the UT334A preset behaves as stock; the copied M4011 factors apply only to other sources.

## 2026-10-26: USB auto power-on; serial and Bluetooth options (offline)
- Feature review vs the other Rad Pro devices: build-flag only = GAME (+3.8 KB flash / +0.6 KB RAM, user: not
  wanted), USB_AUTOPOWER_ON; port work = HV profiles (bench first). Hardware: serial (UART), vibrator (spare
  pin + MOSFET), Bluetooth. Not practical: VOICE (GMC-800 voice chip), EMFMETER (GC-03 sensor).
- **USB_AUTOPOWER_ON** added (Settings → Power → USB auto power on; off by default as upstream). Rad Pro only
  checks it at boot, which on the UT334A is a battery connection: with POWER_OFF_SLEEP, boot powers on only if
  USB is present, and a USB plug-in while off powers on (heartbeat edge in updatePowerState). Emulator (temp
  build, setting on): battery boot → off → DeepSleep 5.37–7.0 s → USB at 7 s → measurement screen with the
  charging icon; boot with USB → measurement screen. Not flashed.
- UART pins (datasheet pin table): **UART0 = PA13 RX / PA14 TX** (QFN 3/4, no tracks) and **UART5 = PD0 RX /
  PD1 TX** (QFN 30/31; stock drives them low and never uses them, continuity unknown). Two independent UARTs:
  wired serial and Bluetooth can coexist without a switch.
- `fm33_comm.c` is still a stub; without DATA_MODE Rad Pro opens comm only while USB powered. Upstream serial
  devices use 115200 8N1.
- IO levels: VDD is 3.0 V; 3.3 V logic (FTDI, HC-05) into the MCU is within the latch-up spec (VI < 1.5 VDD) but
  above VDD: 1 kΩ in series on lines into the MCU. MCU 3.0 V output is a valid high for 3.3 V logic.
- **Serial driver** (`fm33_comm.c`, was a stub): UART0 on PA13 RX / PA14 TX (GPIOA DFS = 0 → UART0, not
  LPUART0), APBCLK (UART0CKS = 00, UART0CKE), baud = APBCLK / (SPBRG + 1) → BGR 555 = 115 108 baud (0.08 %),
  8N1 (CSR 0x43), RX pull-up, RXBF interrupt, TXBE interrupt while sending; error flags write-1-to-clear.
  Same protocol state machine as the STM32 serial driver. Closed: pins analog, clocks off. Opens while USB powered.
  No APBRSTCR reset (needs the RCC_PRSTEN key; all registers are written anyway). UART CSR.IOSWAP swaps RX/TX
  if the wiring ends up crossed.
- `fw_emu.py`: UART0 model (`EMU_UART_RX=tick:text`, '|' = newline; TX printed). With USB from boot:
  `GET deviceId` → `OK UNI-T UT334A;Rad Pro 3.1.2test2/en;<chip UID>`, `GET tubeType` → `OK UT334A`,
  `GET tubeSensitivity` → `OK 68.270`, battery 3.900, pulse count, unknown command → `ERROR`. Model limit: it
  feeds one byte per tick without waiting for multi-part replies (deviceId), so a command right after deviceId
  loses its first bytes in the emulator (a real host waits for the reply line).
- Bench test plan (FTDI): FTDI TX → 1 kΩ → PA13 (QFN 3), FTDI RX ← PA14 (QFN 4), GND; FTDI VCC unconnected,
  3.3 V logic; USB charger plugged in (comm opens on USB power); terminal 115200 8N1, send `GET deviceId` + CR/LF.
- **Off-state charging screen fixed** (plan item 6, "only after a key press"): on other devices a USB plug-in
  boots the MCU and boot() shows the battery screen; on the UT334A the MCU keeps running, so the USB edge only
  requested the backlight and the off view drew with its timer at 0 = a blank, lit screen. Now, while off, the
  USB edge does what boot() would (power on with USB auto power-on, else the battery/charging screen for 5 s).
  Emulator, before: USB at 7.0 s → LCD on, backlight at 7.37 s, blank frame. After: charging icon, backlight
  off again at 11.0 s, green LED on from 7.37 s and stays.
- Display DMA not done: mcu-renderer sends one pixel per send16() call; SPI1 at 32 MHz needs 32 CPU cycles per
  pixel on the wire either way, so DMA would only overlap rendering with the transfer (double-buffered layer
  under mcu-renderer, untimeable in the emulator). No slowness reported: decide at the bench.
- **Low battery as stock** (user decision): power off below 3.35 V held for 100 s (stock: < 3350 mV for 10
  samples in a row, each an 8-sample average every ~10 s, no USB). Rad Pro compared one raw reading per second
  with 3.0 V; at 3.35 V a single dip (alarm, HV burst) would switch off, hence the 100 s persistence. Defines in
  the UT334A block of `settings.h` (power.c does not include the board header: a first version in `ut334a.h`
  was silently ignored, caught by the emulator). Emulator, test build with 5 s: battery 3.29 V from 8 s → last
  HV burst 13.24 s, battery screen to 17.25 s, LCD parked + DeepSleep 17.37 s, counter back to 0.
- getTubeType() upstream bug: user decision, no upstream PR; listed in the fork's `ut334a/README.md` under
  "Changes to shared Rad Pro code" (with every other change outside `src/fm33/`).
- `fw_emu.py`: carries on after 5 M instructions without a WFI (a full-screen image draw at power-on takes
  more: CPU-bound, not SPI-bound); `EMU_DUMP=sym[+off],...` prints RAM words; the UART model waits for the CR LF
  of each reply (deviceId's two-part reply no longer eats the next command).
- Line endings: upstream files are CRLF in the repo; `sed -i` in Git Bash rewrote settings.h as LF (whole-file
  diff). Restored; edit shared files with Python keeping CRLF.
- Fixed 5 s averaging idea dropped (user decision): at ≈ 9 cpm background a 5 s window holds < 1 count, so the
  reading would jump between 0 and ≈ 1.6 µSv/h; Rad Pro's adaptive fast averaging already covers strong sources.
- Committed: fork `0954b16` (charging screen while off, low battery 3.35 V / 100 s, README shared-code table).
  Release build (en) checked: RAM 11964 B, flash 48460 B. Next to flash after "erase it".

## 2026-10-26: fork 0954b16 flashed
- Rebuilt `uni-t-ut334a-en` at fork `acc8b3a` (= `0954b16` + docs), signed (image SHA-256 c234f030…). Emulator:
  boot → off (DeepSleep at 5.4 s) → OK held at 7 s → clean measurement screen, 0 key-error locks.
- Dry run clean (UNPROTECTED, FLS not locked), then flashed on battery with `--no-nrst`: 512 pages erased,
  12166 words programmed, VERIFY OK (readback 3b7e26a6…). Bench checks of the new features still to do.
- Bench, `0954b16`: boots and counts; USB in while on → charging icon; off with USB in → green LED stays on
  (not yet off after the hold); USB out while off → LED off at once; **USB back in while off → LED on, but no
  charging screen** until a key is pressed.
- Cause (from the code, not confirmed on the unit): the USB plug-in edge is detected only in the 1 s
  heartbeat (`updatePowerState`). Unplugged while off, the unit enters DeepSleep at once and SysTick stops, so the
  unplug is never sampled and `previousUSBPowered` stays true → the next plug-in is not an edge. The LED path reads
  PA9 on every loop pass, so it is unaffected. The emulator cannot show it: it runs SysTick on every DeepSleep
  WFI (`EMU_USB=7000:9000:11000`, now on/off/on toggles, shows the screen at 11.0 s).
- User decision: keep as is. Side effects of the stale flag, all cleared by the next key press or power-on
  (heartbeat runs again with USB out): only USB auto power-on (if enabled) would also miss that re-plug.
  Unaffected: charging icon, charging LED, serial (comm), low-battery cut-off, which all read PA9 directly.
- User then asked for the fix. Bench, more: with USB in and the battery full, off → LED off, a key shows a
  green battery (USB, not charging = full; stock-like). USB out → key shows the normal battery icon; **after a
  while keys while off show nothing**. USB auto power-on on: plug-in while off powers on (key press before it).
- Cause of "keys show nothing": `canEnterDeepSleep()` read the raw key pins; a release is debounced over
  KEY_PRESSED_MASK (25) SysTick ticks, so the unit went back to DeepSleep before the release (the off-state
  event) was seen. Worked only while the 5 s battery window kept it awake.
- `fw_emu.py`: `EMU_DEEPSLEEP=real` (no SysTick while PMU_CR.PMOD = 2, as on the chip). Reproduced both on the
  flashed image: left key 8.0–8.15 s while off → no backlight; USB 7 s / out 16 s / in 19 s → LED, no backlight.
- Fix (fork, not committed/flashed): `isKeyboardIdle()` in keyboard.c (POWER_OFF_SLEEP only: no debounced key
  down, no queued event) added to `canEnterDeepSleep()`; `sleepWhilePoweredOff()` calls `updatePowerState()`
  before DeepSleep so an unplug is recorded. Emulator (real DeepSleep), image c0950807…: key → backlight at
  8.55 s; USB re-plug at 19 s → backlight 20.25 s; OK held at 7 s → power-on. 0 key-error locks. +24 B flash.
- Alarm (user, display asleep, source): pulse LED and red alert LED fine, screen flashes (Rad Pro alert flash),
  **no alarm sound**, and the pulse clicks stop each time the screen flashes. Reading: `triggerAlert()` starts
  the backlight flash and a steady buzzer tone (alertTicks) together, and `setBuzzerTimer()` ignores shorter
  pulse clicks while it runs, so silent clicks during each flash mean the alert tone was on but inaudible
  (volume / drive). To check: Settings → Sound → alert volume (selecting a level plays the alarm).

## 2026-10-26: Overview screen (stock-style all-in-one), emulator only
- New measurement view `measurements/overview.c` (flag `MEASUREMENT_OVERVIEW`, UT334A only), first in the view list
  for now: instantaneous rate large (with unit, confidence, alert colours), then Average and Cumulative rows with
  elapsed time. All in the primary dose unit (Sv/rem/cpm/cps). Title reuses STRING_MEASUREMENTS (no new strings).
- The medium colour font only had unit glyphs: `build-fonts.py` now adds `.0-9` and U+2012 to it (fontconv output
  reproduced the old font byte for byte first). en +1 KB. Romanian fonts were stale upstream; left untouched.
- Text bands sized to the full glyphs (g/µ descenders, accents); row units in the small font (medium has no space);
  a too-long key is clipped with an ellipsis before the time (uk "Накопичувальний").
- `fw_emu.py`: `EMU_POKE=tick:sym+off:mask:value` (settings: doseUnits = word 0 bits 13-14, displayTheme = word 3
  bits 3-4). Checked Day/Dusk/Night, cpm, uk, el. Not flashed, not committed.
- Separators in COLOR_INSTRUMENT_FRAME_TERTIARY (the title-bar shadow colour vanished in Dusk/Night); upper line
  4 px higher (confidence band 20 px), second line between Average and Cumulative (user request).
- Screen order (user choice "Overview tabs"): Up/Down cycles Overview ↔ History only (Instantaneous/Average/
  Cumulative views out of the cycle under MEASUREMENT_OVERVIEW). OK cycles the Overview's lower area: Average +
  Cumulative → Max + rate in the secondary unit → bar (→ FAULT when the tube fault alert is on). Reset (Left held
  1 s) clears rate + max, average and cumulative, as stock "Delete Current Data". Rate/dose alerts while the
  display is off switch to the Overview. Emulator: real OK press → Max tab, Right → History, Reset → 00:02 and
  0.000 µSv 2 s later. FS2011/FS5000/GC-03/GMC-800 still build. Not flashed, not committed.
- History: new first tab "3 min" (flag HISTORY_3_MINUTES, UT334A only): 1 s per point, 200 points = 200 s window,
  ticks every 20 s; label generated from each language's "10 min" string (30 files, +1 line each). RAM +220 B
  (12.2 KB). Accuracy unchanged (points are the instantaneous reading); faster averaging rejected (noise at
  background: 10 s ≈ 1.5 counts). Emulator: same 25 s of data is 3× wider than on the 10 min tab.

## 2026-10-27: stock features vs port (from the stock strings, no flashing)
- Stock menus (`analysis/ghidra/strings.txt`): Measure, Schedule (start delay, measurement time), Records (10:
  No., start time, Average, Maximum, Total, duration), Delete (current/history, password), Alarm (rate, total
  dose), Sound (alarm volume, alarm times, particle sounds), Clock, Sleep (screen sleep, auto shutdown hours,
  brightness), Unit (uSv/uGy), Language, ICRP2007 info (0.52 / 10 uSv/h), hidden C1/C2.
- Missing in the port: auto shutdown, µGy label, Schedule start delay, on-screen Records, ICRP page. Decision:
  no stock restore just to look (restore ≈ 23 min, return needs a manual NRST hold; a patched SWD-on stock image
  was judged riskier than holding NRST once).

## 2026-10-27: fork 91b25e6 flashed (off-state fix + Overview + 3 min History)
- Rebuilt `uni-t-ut334a-en` (RAM 12184 B, flash 48976 B), signed image SHA-256 7bdcbd40…01d2b4. Emulator (real
  DeepSleep): key at 8.0 s while off → backlight 8.55 s; USB 7 s / out 16 s / in 19 s → backlight 20.25 s;
  OK held 7 s → power-on, Overview shown; 0 key-error locks.
- Dry run clean (unprotected), then flashed after "erase it", `--no-nrst`, battery power: 512 pages erased,
  12294 words programmed, VERIFY OK. Bench checks pending.
- Bench (user, all OK): power-on → Overview; OK cycles Average/Cumulative → Max + secondary unit → bar → back;
  Right → History on 3 min (1 s/point), Right again → Overview; Left held 1 s resets timers, cumulative and Max;
  Overview in Dusk and Night readable, separators visible; off > 10 s + Left → battery screen (bug a fixed);
  off, USB in → dark → out → in → charging screen without a key (bug b fixed).

## 2026-10-27: alarm tone, narrowed down (bench + code, unfinished)
- Bench (user): Settings → Sound → Alert volume preview plays the alarm at each level (as described). Real rate
  alarm (lowest threshold) **with the display on: tone OK**. **From display sleep: screen flash + red LED, no tone**
  (and, from before, pulse clicks stop during each flash).
- Code: menu preview and real alert share `triggerAlert()` → `setBuzzerTimer(alertTicks, 1, alertVolume)` (GPTIM0
  CCR1 = steady PWM, 250/500 ms). The only display-sleep difference found so far: each alert flash wakes the panel
  (`requestBacklightAlert()` → view redraw → `setDisplayEnabled(true)`: SLPOUT + its delay via `sleep()`/WFI, full
  redraw, then SLPIN after the 0.5 s flash). Nothing in that path writes GPTIM0 or PB10; 8002A SD is tied to GND.
  Hypotheses still open: (1) supply droop when panel + backlight start while the amplifier drives the speaker
  (would need a scope on the battery / 8002A VDD during an alert from sleep); (2) something in the wake path
  re-zeroing CCR1 (check with the emulator: GPTIM0_CCR1 writes during an alert from display sleep).
- Emulator: display-on alarm run (rate alarm poked at 11 s, pulse every 50 ticks) shows normal CCR1 writes.
  Display-sleep run (alarm poked at 48 s) was too slow (> 10 min) and was stopped; next time poke displaySleep
  to 10 s (`settings+12`, bits 7-9 = 0) to shorten it. Settings layout (ARM bitfields, no container straddling):
  word 1 bits 4-7 rateAlarm (index 1 = lowest), bit 18 alertSound, bit 22 alertDisplayFlash.
- On the unit: rate alarm still set to the lowest value; set it back (Settings → Alerts → Rate alarm).
- Session stopped here (internet out); nothing flashed after 91b25e6.

## 2026-10-27: alert wakes the display (stock behaviour), flashed
- User, stock behaviour: display asleep + high radiation → pulse LED blinks with the display dark; when the alarm
  hits, the unit wakes the display and both the tone and the pulse LED run.
- Emulator, alert from display sleep on 91b25e6: GPTIM0_CCR1 tone 250 ms every second, PB10 DFS/FCR and
  RCC_PCLKCR4 GT0 untouched by the backlight re-init each flash → the register side is correct; the silence is
  on the hardware side during the per-flash panel wake (cause not identified).
- Fork change (not committed): `ALERT_WAKES_DISPLAY` (UT334A only): `triggerAlert()` calls
  `requestBacklightTrigger()` when `!isDisplayAwake()`, else the usual 0.5 s flash. Emulator: display wakes at
  19.1 s and stays on for the display-sleep time; tone unchanged. Side effect: if the alert lasts past the sleep
  time, ≈ 0.6 s dark gap, then a new wake. +16 B flash.
- Flashed after "erase it" (dry run clean, `--no-nrst`), signed image b4674e27…, 12298 words, VERIFY OK.
  Bench check pending.
- Bench (user): alarm from display sleep → display wakes and stays on, tone audible, pulse LED keeps blinking.
  Works. A short key press does not acknowledge (Rad Pro: only Left held 1 s = EVENT_KEY_RESET, which while an
  alert is pending only clears alertPending); the hold works. Stock acknowledgement not remembered; user keeps
  the Rad Pro behaviour. Alarm step closed.

## 2026-10-27: battery current (fork 383dc38, meter in series with battery +)
- Off (DeepSleep, BOR off): **3–8 µA** (the higher values = the 1 s RTC wake-ups). Close to the datasheet ≈ 2 µA;
  no leak path while off. Step closed.
- On, idle (display on): ≈ 40 mA. During an alarm: up to ≈ 365 mA peak (8002A driving the speaker with the
  full-duty square wave at the selected alert volume, plus backlight); bursts of 250 ms per second, so the average
  is far lower. Display-asleep current not measured yet.

## 2026-10-27: settings and RTC across a battery disconnect
- Bench (user): theme (Night) kept after power-off + 10 s battery disconnect → settings in flash survive.
- RTC: started from "00:24" after the disconnect. No backup cell: the FM33 RTC BCD registers reset to 0 (SVD reset
  values), i.e. 2000-00-00 00:00:00, an invalid date (month 0, day 0) that the port never corrected (stock writes a
  default date, `0x8330`).
- Fork fix (not committed/flashed): `initRTC()` (every power-on) sets 2025-01-01 00:00 UTC (RTC_YEAR_MIN, the
  menu's lowest year) when month or day is 0 or the year is below 25. Emulator (RTC reads 0): one write at
  power-on, BCD 25-01-01, weekday 3 (Wednesday). Signed image ed14484d…, +60 B.
- User: batch it (no flash now); goes with the battery calibration (step 5) in the next flash.

## 2026-10-27: serial moved to UART5 (PD0/PD1), not flashed
- User soldered the serial wires to **QFN 30/31 = PD0/PD1** (PA13/PA14 not reachable). Datasheet pin table:
  PD0 = UART5_RX, PD1 = UART5_TX (their only digital function, so DFS = 0, assumed; the bench test confirms).
- Fork: `USART_*` in ut334a.h → UART5, PD0/PD1, UART5_IRQn, `USART_PCE_Msk` (PCLKCR3 bit 13). UART5 has no
  kernel clock select (APBCLK only), so the OPCCR1 UART0CKE/CKS lines were dropped. Baud, 8N1, IRQ code unchanged.
- `fw_emu.py`: the UART model follows the UART with its own IRQ handler (UART5 now, UART0 in older builds).
  Emulator, image 15eec4ab… (includes the RTC fix): USB from boot, `GET deviceId` → `OK UNI-T UT334A;Rad Pro
  3.1.2test2/en;060808091b04ff0a0066003e`, `GET tubeSensitivity` → `OK 68.270`; PCLKCR3 0x2001, GPIOD FCR PD0/PD1
  = 10 (digital function), DFS 0.
- Bench wiring: FTDI TX → 1 kΩ → QFN 30 (PD0, RX); FTDI RX ← QFN 31 (PD1, TX); GND; FTDI VCC unconnected,
  3.3 V logic; USB charger in (comm opens on USB power); 115200 8N1.
- Flashed after "erase it" (SWD first failed: no connection within 180 s; user re-seated, dry run clean): image 15eec4ab… (UART5 + RTC fix), 12306 words, VERIFY OK. Settings reset by the erase.
- Bench serial (FTDI COM7, 115200 8N1, USB charger in): first try echoed every command back = QFN 30/31 shorted
  (user fixed it); then no reply until the user **swapped the FTDI TX/RX wires**; then works:
  `GET deviceId` → `OK UNI-T UT334A;Rad Pro 3.1.2test2/en;060808091b04ff0a0066003e`, `GET tubeSensitivity` →
  `OK 68.270`, `GET deviceBatteryVoltage` → `OK 4.238` (charging), `GET tubeRate` → `OK 8.508` cpm,
  `GET datalog` works. UART5 on PD0/PD1 with DFS = 0 confirmed.
- RTC after the battery disconnect (continuity check): `GET deviceTime` = 1030885128 = **2002-09-01 12:58 UTC**,
  first datalog entry after power-on 2002-09-01 12:56:46; older entries 2005-05-08. So the RTC came back with a
  BCD-valid but wrong date (year 02), and the new `initRTC()` check (year < 25 → 2025-01-01) did **not** take
  effect at power-on, although `SET deviceTime` over serial works (set 1793130942 = 2026-10-27 19:55:42 UTC,
  read back, counting). Open: was the RTC write at power-on ignored (32 kHz crystal not yet running after POR?),
  or not reached? To check: battery out 10 s, in, SWD read of RTC_BCD* before power-on, then power on, read again.
- SWD (read-only, firmware running) after battery out 10 s with FTDI attached, unit left off: RTC BCD 2026-10-27
  20:06 (kept, still counting), `currentTick` 9.4 s → the MCU **rebooted** but the RTC kept its time. Repeated with
  **everything disconnected** (FTDI, USB, battery): RTC still 2026-10-27 20:10, tick 5.7 s (fresh boot). So the
  FTDI back-power idea is wrong: the board's own capacitance holds VDD high enough for the RTC (µA in DeepSleep)
  through a short disconnect while the MCU resets. The first "00:24" (old firmware) and the "2002-09-01" are
  then a never-valid RTC and a brown-out-corrupted RTC; why the year-02 case was not fixed at power-on is open.
- RTC fix verified on hardware: SWD write of BCDYEAR = 0x02 (WER key 0xACACACAC, RTC register only), unit
  off/on → RTC 2025-01-01 00:00:10, weekday 3. (Note: the check runs only at power-on; a date scrambled while the
  unit stays on is not corrected, which may explain the 2002 case.) Clock then set over SWD to UTC
  2026-10-27 20:15:34; time zone setting is UTC after the erase.

## 2026-10-27: end of fourth session
- Pushed: fork `ad932fc` (public, branch ut334a; checked: no backup/analysis/binaries in the new commits) and the
  private workspace. On the unit: `ad932fc` (image 15eec4ab…).
- User decisions: step 6 (GM calibration) dropped: only an Am-241 smoke-detector source (no known dose rate),
  no reference meter. Remaining: step 9 (stock features, auto shutdown + µGy first), then step 5 (battery
  calibration) as the final step; after that the project is closed. Next session starts with step 9.

## 2026-10-28: step 9 part 1, auto shutdown + µGy (code + emulator, not flashed, not committed)
- User choices: auto shutdown counts from power-on; options Off/1/2/4/8/12/24 h, default Off.
- `DOSE_UNITS_GRAY` (UT334A): unit "Gray" (Gy/h, Gy) in Dose units and Secondary units, same values as Sv (stock
  only swaps the label: its ICRP strings give 0.52 for uSv/h and uGy/h). `doseUnits`/`secondaryDoseUnits` widened
  to 3 bits (validated on load); settings size unchanged.
- `AUTO_SHUTDOWN` (UT334A): Settings → Power → Auto shutdown. Counts heartbeats on battery since power-on; restarts
  on USB unplug and on a setting change; held while an alarm is active; then `powerOff()` (settings saved).
- Strings in all 30 languages; `build-fonts.py` adds STRING_GY* to the medium (unit) font set; fonts regenerated
  (74 files, other devices' fonts also gain the glyphs). Glyph check: all new strings covered in all 30 UT334A
  languages. `tools/platformio.ini.base` synced (it lacked MEASUREMENT_OVERVIEW, HISTORY_3_MINUTES,
  ALERT_WAKES_DISPLAY). Builds: all 30 UT334A languages, GC-03 en, FS2011 en/zh_CN. en: 49.6 KB flash, 12.2 KB RAM.
- New emulator pokes: primary unit `settings:0xe000:<v>` (cpm 0x4000, Gy 0x8000; secondary at 0x70000);
  auto shutdown `settings+12:0x38000000` (1 h = 0x08000000); counter `power+16` (enums are 1 byte). Power on in the
  emulator with OK held (`key_mask 0x400`, release 1300), else it powers off at tick ≈ 5000.
- Emulator: Overview shows µGy/h, Average µGy/h, Cumulative µGy. Auto shutdown 1 h with the counter poked to 3590 s
  at tick 4000: DeepSleep at tick 14370 (≈ 10 s). With USB in, or setting Off: no shutdown in 20 s.
- Flashed 2026-10-28: fork `65d937a` (en, image 49d36b88…), dry run OK (UID 060808091B04FF0A0066003E, unprotected),
  512 pages erased, 12455 words, VERIFY OK, `--no-nrst`, battery power. Settings reset by the erase.
- Bench (user): boots, Gray unit works, "everything works fine". Auto shutdown kept as flashed (default Off), no
  bench test (user choice). Step 9 closed (Schedule, Records, ICRP page not wanted). Remaining: step 5.

## 2026-10-28: end of fifth session
- Docs: fork README status (2026-10-28), port README shared-code table (overview, 3 min history, alert wake, Gray,
  auto shutdown, strings/fonts), install guide and hardware map serial → UART5 PD0/PD1 (were still UART0).
- Line endings: the step 9 edit had turned 5 CRLF sources LF (whole-file diffs vs upstream); restored to CRLF
  (content unchanged, image unaffected).
- User: day-long test of `65d937a` next, issues reported in a new session; then step 5.

## 2026-10-29: step 5, battery calibration, second point
- User: DMM 3.506 V across the battery vs Settings → Statistics → Battery 3.550 V (USB out). First point
  (2026-10-27): 4.029 V vs 4.073 V. Both read +44 mV high (+1.09 % / +1.25 %).
- Gain-only fit: ×0.9884 → divider 0.5438 → 0.5502; residuals −3.5 mV at 4.03 V, +2.6 mV at 3.51 V. An offset
  model (−44 mV) fits both exactly, but two points at ~±2 mV meter/display resolution cannot tell them apart.
- 2026-11-01: the gain fit (0.5502) matches the stock firmware's divider constant 0.55. With 0.55: 4.027 V
  (meter 4.029) and 3.510 V (meter 3.506), both within 4 mV. Proposed: `PWR_BAT_SCALE_FACTOR` = 1 / 0.55
  (gain only, no offset term), build, dry run, flash after "erase it", check one point.
- Built: fork `ut334a.h` `PWR_BAT_SCALE_FACTOR` = 1 / 0.55 (not committed), en 49620 B, signed image
  c5b6d270…. Dry run `--no-nrst`: no SWD connection within 180 s (ST-Link/unit not connected?). Not flashed.
- User: flash later (next session). Handover in CLAUDE.md.

## 2026-11-01: optional add-on, wireless serial (research only, nothing built)
- User asked: USB-UART bridge vs ESP32 vs BT-05. BT-05/HM-10 are BLE: no COM port on Windows (GeigerLog,
  radpro-tool need one). Classic SPP (HC-05, JDY-31) gives a COM port on Windows and works with Android
  "Serial Bluetooth Terminal". USB bridge = no battery cost, but D+/D− on the unit's connector are unknown.
- makerselectronics.com (2026-11-01): HC-05 195 EGP in stock (VCC 3.6–6 V breakout, 3.3 V logic, ~50 mA,
  9600 default); JDY-31 130 EGP out of stock (SPP, 1.8–3.6 V, 4.7/7.3 mA); HM-10 345 EGP and PB-03 are BLE.
- Power fit: comm opens only while USB powered (`updateComm`); Rad Pro's `DATA_MODE` (FS2011: Settings → Power →
  Data mode) would open it on battery. A module would need a switched supply (free pin, e.g. PA13 QFN 3,
  no tracks) via N-MOSFET → P-MOSFET as PC4/HVbatt, off in DeepSleep. Not decided.
- User decisions: HC-05 (in stock). UART5 (PD0/PD1) goes to the HC-05 (PA13/PA14 not solderable), so no
  wired serial afterwards. Proposed switch (store parts): AO3401 P-MOSFET VBAT → HC-05 VCC, gate 100 kΩ to
  VBAT, pulled down by an AO3400 N-MOSFET (gate from MCU via 1 kΩ, 100 kΩ to GND). Control pin: PC9 (QFN 22)
  or PC10 (QFN 23), free in stock; tracks unknown → continuity check first.
- User: HC-05 power from the "HV enable" (PC4, HVbatt switch) instead of a new pin. PC4 is high whenever the
  tube is enabled (unit on, `setTubeHVEnabled`), low in DeepSleep; `readBatteryVoltage()` also pulses it while
  off (charging LED). So the module is on whenever the unit is on (no Data mode off). Risks if VCC is on the
  HVbatt rail itself: drop across the "39K" P-MOSFET (Rds unknown) lowers the battery reading (divider is on
  HVbatt), HC-05 current pulses on the LM3478 supply (false counts?). Option B: PC4 only as gate drive for an
  own AO3400/AO3401 switch from VBAT. Bench checks proposed before choosing.
- User: back to a dedicated control pin for software on/off (PC4 dropped). Candidates PC9 (QFN 22) /
  PC10 (QFN 23); continuity check first. Firmware plan: `DATA_MODE` (Settings → Power → Data mode) switches
  the HC-05 supply and opens/closes UART5; module off in DeepSleep.
- Plan saved: `docs/bluetooth-plan.md` (hardware edits, firmware edits, tests, open decisions). Bluetooth only.

## 2026-11-01: off-state keys silent (user request)
- User: while off, no key shows the battery screen; only the OK long press (power on) and a charger plug-in
  (charging screen) do anything. To be flashed together with the step 5 battery fix.
- Fork (not committed): `keyboard.c` key release while off queues no `EVENT_KEY_TOGGLEBACKLIGHT` under new flag
  `POWER_OFF_KEYS_SILENT` (UT334A section of `platformio.ini` and `tools/platformio.ini.base`). Battery
  connection (boot) and low-battery screens unchanged. Build en OK: 49604 B flash, 12212 B RAM. Not signed,
  no emulator check yet (to ask).
- Power question (user): keys are not polled while off: they are WKUP edge wake-ups; the only periodic wake is
  the RTC 1 s interrupt for the IWDT (4 s, cannot be frozen with the factory option bytes). Option: drop the
  Left/Right wake-ups, keep OK; saving only on accidental presses (not measured).
- Emulator (real DeepSleep), build with `POWER_OFF_KEYS_SILENT`: boot → battery screen → DeepSleep; at 9.0 s
  short press (150 ms) of Left, OK, Right: no display command after 8.5 s, unit stays in DeepSleep (all three).
  OK held 9.0–13.0 s: display init at 10.1 s, power-on, Measurement view. USB at 9.0 s: display init at 10.0 s,
  charging screen (green charging icon). 0 key-error locks.
- User: only OK wakes the unit while off. `fm33_power.c`: WKUP wake-up for the OK key only; DeepSleep check
  looks at the OK pin only (a held Left/Right no longer keeps the MCU awake). Build en: 49532 B flash.
- Emulator (real DeepSleep), same cases: Left / Right short press and Left held 4 s → no display command,
  10633 ticks in DeepSleep (OK short press 10453, previous build Left 10453: the 180 ms awake are gone, i.e.
  Left/Right no longer wake the MCU). OK short: nothing visible. OK long: power-on at 10.1 s. USB: charging
  screen at 10.0 s. 0 key-error locks.

## 2026-11-01: flashed battery fix + silent off-state keys + OK-only wake
- After "erase it": signed en image 66fd43a0… (fork uncommitted: divider 0.55, `POWER_OFF_KEYS_SILENT`,
  OK-only wake). Dry run `--no-nrst`: unprotected, UID 060808091B04FF0A0066003E. Flash `--no-nrst`, battery
  power: 512 pages erased, 12433 words, VERIFY OK. Settings reset by the erase. Bench checks pending.
- Bench (user, 2026-11-01): "all good, works as described": battery reading vs DMM, short presses of Left /
  Right / OK while off show nothing, OK long press powers on, USB plug-in shows the charging screen.
  **Step 5 closed.** Fork commit `5531898` (= the flashed image).

## 2026-11-01: end of sixth session
- On the unit: fork `5531898` (battery divider 0.55, keys silent while off, only OK wakes from off).
- Main port closed (steps 5 and 9 done; 6 and 8 dropped). Open, optional: HC-05 Bluetooth add-on,
  `docs/bluetooth-plan.md`. Next for it: H2 continuity check of QFN 22/23 (control pin), H1 HC-05 at 3.5 V,
  user decisions listed in the plan (Data mode after power-on, low battery, menu label, VBAT tap point).

## 2026-11-01: Bluetooth interference review
- Tube not RF-sensitive; risks are VBAT/HVbatt dips from HC-05 bursts (HV shift, low battery reading, early
  low-battery shutdown), small RF pickup risk, and mechanical contact/arcing near the HV section. Added to
  `docs/bluetooth-plan.md`: placement/insulation rules, decoupling fallback, T6 and T7 while streaming, T7a
  (scope PA8 while streaming). HC-06 raised by the user as an alternative to the HC-05 (open).

## 2026-11-01: H2 continuity (user), HC-05 chosen
- Module: HC-05 (HC-06 considered, not taken).
- QFN 22 (PC9): hard-wired to GND. QFN 23 (PC10): free. **Control pin = PC10 (QFN 23).** The port does not
  configure PC9/PC10 (checked `src/fm33/`); PC9 must never be driven high.

## 2026-11-01: H1 HC-05 alone on a supply (user)
- Starts from **3.4 V**; below, LED dark, nothing works (breakout spec 3.6 V). Margin to the firmware's
  low-battery shutdown (3.35 V) is ≈ 50 mV before switch and wiring drops. Drop-out voltage of a running,
  paired module not measured yet.
- Start-up test was repeated power cycles from 3.3 V up: echo (loopback) works from 3.4 V.
- Drop-out test (paired, echoing at 3.7 V, supply lowered): **unresponsive from 3.5 V**, higher than the 3.4 V
  start-up. Preliminary: supply current limit, lead drop and the meaning of "unresponsive" still to confirm.
- Drop-out details (user): connection lost in the serial app; supply has no current limit, draw 40–68 mA;
  3.5 V read on the supply display (not at the module pins).
- User: bypass the breakout's regulator with a lower-dropout LDO. makerselectronics (searched 2026-11-01):
  3.3 V LDOs only XC6206P332MR (SOT-23), AMS1117/XBL1117/LM1117-3.3 (≈ 1 V dropout, worse); SPX3819 only in
  5 V / 1.2 V; LP2951CM (adj., SOIC-8, not better at 70 mA); no AP2112, ME6211, MCP1700, RT9013, HT7333.
  XC6206 is the usual regulator on HC-05 breakouts ("662K"), so a swap may gain nothing. Next: identify the
  breakout's regulator and any series diode, and measure its output while running.
- Bench (user): supply at 3.4 V (module running) → breakout regulator output **3.0 V**: ≈ 0.4 V lost
  between supply and core (leads, possible series diode, regulator: not split yet). Core runs at 3.0 V.
- Bench (user): the ≈ 0.4 V is lost in the breakout regulator alone (no lead/diode loss).
- Battery at 3.5 V: Rad Pro's lowest Li-ion level boundary is 3.527 V (`system/power.c`), so the icon is
  already at its bottom level; a typical Li-ion cell has ≈ 5–10 % left at 3.5 V (generic curve, this cell
  not measured). Bluetooth drop-out at 3.5 V thus costs roughly the last 5–10 % before the 3.35 V shutdown.
- **Decision (user): keep the breakout regulator; firmware auto-off of Data mode below ≈ 3.55 V on battery.**

## 2026-11-01: HC-05 on the unit's supply (user): Bluetooth dropped
- Setup: HC-05 VCC from the unit (tried HVbatt, then VBAT), UART **not** connected to the MCU (module RX/TX
  jumpered as a loopback). Every message through the module gave false counts/pulses and set off the alarm.
- 22 µF across the module did not help; 100 µF mostly fixed it, but small power dips remained.
- With no data wiring, the disturbance must come from the module's supply current bursts and/or its radio, not
  from the UART. Not split further (no separate-supply test).
- **Decision (user): Bluetooth shelved.** `docs/bluetooth-plan.md` kept for reference only. Open need: easy
  data extraction on phone and PC (see next entry once decided).

## 2026-11-01: data extraction over the charging port (user answers)
- Idea: USB-serial chip inside the unit, powered from VBUS only, on UART5 (PD0/PD1), data lines to the charging
  connector. Connector: **USB-C**. D+/D− of the connector are **not wired to anything**; using them needs manual
  wiring. Back-power protection (series resistors, UART off while PA9 low) to be added (user).
- User: phone is **Android**; USB-C has the 5.1 kΩ CC resistor; no room for a PCB near the port, so D+/D−
  run ≈ 10 cm to the module.
- makerselectronics (searched 2026-11-01): CP2102 5-pin module (120 EGP, 42 × 15 mm, USB-A), PL2303 module
  (60 EGP), CP2102 6-in-1 (210 EGP), bare CH340C/CH340G SOP-16 (35/20 EGP), OSEPP FTDI (960 EGP); no CH340
  breakout module. Proposed: **CP2102 5-pin**, USB-A plug removed, D+/D− as a twisted pair.

## 2026-11-04: CH340G chosen (user), level shifting
- User: **CH340G** instead of the CP2102. Needs an external 12 MHz crystal (the CH340C does not).
- FM33 datasheet: only **PB12 is 5 V tolerant** (PD0 is not); VIH = 0.7 VDD = 2.1 V at VDD 3.0 V.
- Options: (A) CH340G on VBUS 5 V, divider on CH340 TXD → PD0 (2.2 k / 2.7 k, ≈ 2.8 V); divider pulls PD0
  low when unplugged (constant break), so it needs the firmware step U4. (B) CH340G on 3.3 V from an XC6206P332
  LDO, V3 tied to VCC, 3.3 V I/O, no divider. Proposed: B. Test: PA9 must stay low with the cable unplugged
  (back-power through the UART lines into VBUS).
- User: only **1 kΩ** resistors on hand. A: divider 1 k / 1 k → 2.5 V (2.4–2.6 V over VBUS 4.75–5.25 V,
  VIH 2.1 V). PD1 → RXD: 2 × 1 k in series (≈ 1.2 mA back-power limit unplugged). B: 1 k on TXD → PD0,
  2 × 1 k on PD1 → RXD.

## 2026-11-04: CH340G built (user): works, alarm on USB connect
- Unit responds over USB. **On USB connect the alarm went off completely** (false counts suspected). Not
  isolated yet. Hypotheses: USB bus activity (SOF every 1 ms once enumerated, 12 MHz crystal) coupling into the
  GM pulse path (PA8) from the 10 cm pair or the module placement; CH340 current on VBUS. Next: one-state tests.
- SWD live read (read-only, no halt; `tubePulseCount` 0x200005E0, `currentTick` 0x200005DC, `tubeDeadTime`
  0x200005FC from the 5531898 ELF): before the test, count 7839 at ≈ 696 s uptime (background 9 cpm
  would give ≈ 105) and shortest pulse interval since boot **6 µs** (not a real GM pulse): ≈ 7500 false
  counts happened earlier, in fast bursts.
- Unplug → plug into PC (no serial app, user): 10 counts in the first 28 s (≈ 21 cpm), 1 in the next 30 s;
  11 in 58 s ≈ 11 cpm overall (expected ≈ 9 at 9 cpm: normal overall; the first 28 s alone are high, P(≥ 10 | 4.2) ≈ 1 %, borderline), no burst; a short
  unplug/replug at 28–31 s gave none either. SWD link lost at ≈ 58 s after the plug (cause unknown).
  So plugging in alone did not reproduce the flood this time.
- User: the alarm event was with the **Arduino IDE serial monitor** open, PC + battery only, **ST-Link not
  attached**; it also showed a high radiation reading. Repeating the terminal with the ST-Link attached: no alarm.
- SWD read afterwards: tick 101293 ms, count 21, min interval 905 µs → **the unit had restarted** ≈ 101 s
  earlier (fits the SWD loss ≈ 58 s after the plug); cause unknown (asked user). 21 counts / 101 s ≈ 12.5 cpm (expected ≈ 15 at 9 cpm: normal).
- User: the restart was theirs (off/on and battery replugged), not a fault. After it the issue did not come
  back. **Open:** user will try to reproduce the flood. On return: read count/tick/min interval over SWD
  (values above) before and after; suspect the ST-Link GND as a masking second ground path.
- Wiring as built (user): USB-C D+/D− → CH340G UD+/UD−, **one D pair only** (the other receptacle pair is not
  bridged), USB 5 V and GND → CH340G. PD1 (FM33 TX) → 1 kΩ → CH340 RXD. CH340 TXD → 1 k/1 k divider → PD0.
  Extra GND wire MCU GND ↔ CH340 GND (return path for TX/RX). Option A (CH340G at 5 V).
- Notes: data only works with the plug in one orientation (charging works both ways). PD1 has 1 kΩ, not 2 kΩ:
  back-power when unplugged up to ≈ 2.4 mA; PA9 read 0 when unplugged (2026-11-04, ST-Link attached). The extra
  GND wire closes a loop with the USB GND path: a suspect to remove if the false counts come back.
- Old background, **stock firmware, June 2025** (user photos, readings confirmed by the user; date/time as shown,
  correct per user):

  | Date / time | Real-time µSv/h | Average µSv/h | Duration | Total dose µSv |
  |---|---|---|---|---|
  | 2025-06-11 09:3?:?0 (glare) | 0.10 | 0.13 | 011:00:44 | 1.46 |
  | 2025-06-14 01:14:29 | 0.10 | 0.13 | 002:31:46 | 0.35 |
  | 2025-06-15 02:10:07 | 0.13 | 0.12 | 001:07:20 | 0.14 |
  | (date unreadable) | 0.14 | 0.13 | 024:09:30 | 3.18 |
  | 2025-06-18 10:16:16 | 0.08 | 0.13 | 058:19:50 | 7.68 |
  | 2025-06-20 15:20:51 | 0.08 | 0.13 | 112:20:36 | 14.82 |

  Dose / duration = 0.125–0.138 µSv/h for every photo; longest run 14.82 µSv / 112.34 h = **0.132 µSv/h**
  ≈ **9.0 CPM** at the UT334A preset (68.27 CPM per µSv/h), matching the 9 CPM (2026-10-25) and 8.5 CPM
  (2026-10-27) seen under Rad Pro. Reference for spotting false counts: background ≈ 9 CPM ≈ 0.13 µSv/h.
- User could not reproduce the flood. Next: unit closed, USB to PC overnight (soak test). Check in the morning:
  average/total dose vs ≈ 0.13 µSv/h, alarm, and the Rad Pro datalog over the new USB link (when any burst hit).
- Soak started. **On plugging the USB in: many pulses that sounded like 50 Hz hum** (user). Hypothesis (not
  confirmed): mains leakage from the PC (this PC is a laptop; an unearthed charger puts ≈ half-mains AC through
  Y-capacitors on its ground) reaching the GM input once the battery-floating unit is tied to PC ground. Fits the
  first flood (≈ 7500 counts ≈ 150 s at 50/s). Test: laptop on battery (charger out), then on an earthed outlet.
- User: the hum happened **while holding the unit**; it stopped once set down (soak running, plugged in). Fits
  the hypothesis: the hand closes the path from the PC's AC-leaky ground through the unit to earth. Would also
  explain why it was intermittent. Later test: hold the unit while plugged in, laptop on charger vs on battery.

## 2026-11-04: end of seventh session
- On the unit: still fork `5531898` (no firmware change this session). Hardware: internal CH340G USB serial
  (`ut334a-hardware.md` §7). Bluetooth dropped.
- Open: overnight soak result; confirm the 50 Hz hum cause (held unit, laptop on charger vs battery); fix
  choice; optional firmware U4 (UART5 off while PA9 low); Android OTG test.
- Added `tools/swd_pulse_watch.tcl` (read-only live pulse counter watch, waits for USB unplug → plug).

## 2026-11-04 (eighth session): overnight soak result
- First serial try at ≈ 07:10: COM18 (CH340) opened but Rad Pro sent nothing (LF and CRLF, DTR/RTS off and on).
  User did something on the unit (not stated), then all reads worked (`GET deviceId` → `UNI-T UT334A;Rad Pro
  3.1.2test2/en`, battery 4.189 V). Cause of the silent first try unknown.
- `GET tubeRate` 292.6 cpm at 07:14, falling to 58.8 within 20 s, then ≈ 1 count / 5 s: a burst just before.
- Datalog (`GET datalog`, saved in the session scratchpad): RTC was set ≈ 01:09 (log walks 2003 → 2026-11-04),
  then 1 min logging. **01:54 → 07:03 (5.15 h, untouched on USB): 2859 counts = 9.25 cpm**, per-minute
  variance 10.7 ≈ mean (Poisson), max 20/min: **clean, matches the 9 cpm background.** Only bursts:
  01:53 (332 counts in 1 min) and 07:03–07:06 (263, 199, 694 counts/min), i.e. at the start and end
  of the soak, when the unit was probably handled (to be confirmed by the user).
- So the USB link alone, with the unit untouched, gives no false counts; supports the hand / mains-leakage hypothesis.
- Older datalog segments carry wrong RTC years (2002, 2088, 2092, 2102); cause not checked (likely earlier runs before the clock was set).

## 2026-11-04: 50 Hz hum cause confirmed (laptop charger leakage)
- Live serial log, 2 s steps (`GET tubePulseCount`, `GET tubeRate`), unit on USB to the laptop, held by the user.
- State 1, laptop **on its charger**, unit held: hum heard. 07:16:01 → 07:16:23: 13644 → 13882 = **238 counts
  in 22 s ≈ 650 cpm** (rate peaks 2460 and 2890 cpm), in bursts (2 s steps of 0 to 52 counts).
- State 2, **charger pulled**, still held: hum stopped (user). From 07:16:25: **0 counts** over the following
  8 s+ (the displayed rate holds the last value until the next pulse).
- Conclusion: the false counts come from the laptop charger's leakage onto the laptop ground (Y-capacitors, unearthed
  charger) reaching the GM input through the USB GND when the hand gives a path to earth. Untouched or laptop on
  battery: clean (overnight 9.25 cpm). Fix options: usage rule, USB isolator, or GM input filtering (to decide).
- **Decision (user): fix option 1, usage rule.** Do not hold the unit while it is plugged into a PC whose
  charger is connected (or any unearthed mains-powered host). No hardware or firmware change for this fault.

## 2026-11-04: user findings, stock vs port pulse counting, firmware U4 (not flashed)
- **First silent serial read explained (user):** the wires soldered on the QFN (UART5 PD0/PD1) had broken
  off. User resoldered them and fixed them in place with UV solder mask.
- **Hum coupling (user):** with the laptop on its charger and the unit plugged in, just bringing a hand
  **close to the GM tube** triggers the false counts (no touch needed). So the hand couples capacitively to the
  tube / GM input while the unit sits at the charger's leakage potential.
- User does not recall this with the stock firmware, nor before the mod (whether the stock firmware was ever held
  near the tube while charging from the laptop on its charger: unknown).
- **Stock vs port difference (static analysis, `analysis/ghidra/decompiled_resolved.c` 0x2750):** the stock COMP
  ISR counts a pulse only if COMP2_CR.CMP2O (bit 8) is **still high when the ISR runs**
  (`(COMP2_CR & 0x100) == 0x100`; 0x100 comes from `COMP_ICR >> 22`). The port (`fm33_tube.c`) counts every
  interrupt. Narrow spikes (charger switching noise riding on the 50 Hz leakage) can set the edge flag and be
  gone before the ISR reads CMP2O: stock drops them, the port counts them. Fits the 6 µs / 25 µs shortest
  intervals seen (a real GM pulse stays above 0.6 V for ≈ 45–50 µs). **Hypothesis, not tested.**
  Proposed firmware step U5: same CMP2O check in the port ISR. Test: held near the tube on the charger,
  counts with U5 vs without.
- **Firmware U4 committed, not flashed** (fork `99552fe`, build `uni-t-ut334a-en` OK, bin SHA-256
  `5c12a788…aaf1ac8c`, RAM 12212 B, flash 49604 B; SWD addresses unchanged: `tubePulseCount` 0x200005E0,
  `currentTick` 0x200005DC, `tubeDeadTime` 0x200005FC). Finding: Rad Pro's `updateComm()` already opens UART5
  only while PA9 is high (no `DATA_MODE` in the UT334A env), and `closeComm()` sets PD0/PD1 analog. Gap fixed:
  `sleepWhilePoweredOff()` now calls `closeComm()` before DeepSleep, in case PA9 dropped between `updateComm()`
  and the sleep check (UART5 open in DeepSleep = PD1 high feeding the CH340G through 1 kΩ).
  Flash on the next unit connect (battery power, `--no-nrst`, only after "erase it").
- Unit now disconnected for some time (user). User will run the Android OTG test and report.
- **Decision (user):** U5 (CMP2O check) on hold. First a **full stock vs port comparison** (every stock ISR and
  driver against `fm33/`) to find any other leftovers like this, then fix them together.

## 2026-11-04: end of eighth session
- On the unit: still fork `5531898`. Committed, not flashed: `99552fe` (U4). Unit disconnected for now.
- Next: Android OTG tests (user, new session); flash `99552fe` on the next connect; full stock vs port
  comparison; then U5 and other fixes.
