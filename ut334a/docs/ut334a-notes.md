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
