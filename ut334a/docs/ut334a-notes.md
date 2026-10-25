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
