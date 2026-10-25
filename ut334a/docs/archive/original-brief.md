# Project: Port Rad Pro firmware to the UNI-T UT334A Geiger counter

You are helping me port **Rad Pro** (https://github.com/Gissio/radpro, MIT) to the **UNI-T UT334A**, which Rad Pro does not support. Work in phases and **stop for my confirmation at the end of each phase**. I have an **ST-Link V2 (clone)** connected to the device's SWD pads, OpenOCD 0.12+, and a bench (oscilloscope, multimeter, logic analyzer).

The goal of Phase 1–2 is to get the stock firmware and extract every hardware fact from it, so the HAL is written from facts, not guesses.

---

## 1. What we already know about the hardware

**Board:** silkscreen `GeigerCounter -FM-V0.0`, `WB228802`. Single main PCB, 3.7 V 1800 mAh Li-ion, USB-C.

| Part | Marking | Notes |
|---|---|---|
| MCU | **Fudan Micro FM33LC043N** | Cortex-M0, 64 MHz, 256 KB flash @0x00000000, 24 KB RAM @0x20000000, QFN-32. **"N" variant = no USB** ("U" variants have USB). Not register-compatible with STM32 (except timers, which are STM32-style: CCMR/CCER/PSC/ARR). |
| HV supply | **TI LM3478MA** ("L3478 MA", SOIC-8) | Boost controller → 1.5 mH inductor ("152") → 6-diode multiplier ladder. **HV is regulated in hardware; no MCU PWM.** Open question: does an MCU pin drive LM3478 pin 7 (FA/SD = shutdown) as an HV enable? |
| Crystal | T302A, 2-pad | Almost certainly 32.768 kHz (RTC). Main clock is internal RC/PLL. |
| Display | FPC `FT-02011-FPC-A(#1)`, 10 pins, 2.0" color TFT | 10 pins ⇒ 4-wire SPI. Almost certainly ST7789-family, 240×320. |
| Audio amp | `8002A` (SOP-8) | Mono class-AB amp → 8 Ω 1 W speaker. Pin 1 = shutdown (likely MCU-driven). |
| LDO | `65ZV` SOT-23 + 2 caps | Probably 3.3 V regulator. |
| Charger | SOT-23-6 next to battery connector | Probably TP4057-class Li-ion charger; CHRG/STDBY may go to MCU. |
| GM tube | glass tube, marking read as "J442y XRHH 2023" | Maker XRHH (also makes J305/J321). Model not confirmed; marking likely misread. |
| SWD | row of 5 pads left of the MCU (square = pad 1) | **Measured:** pads 1–3 go directly to MCU pins (SWDIO, SWCLK, NRST in an order still to be confirmed), pad 4 = VDD (**3.0 V**, so the LDO is a 3.0 V part), pad 5 = GND. |

**FM33LC protection facts:**
- Option bytes at `0x1FFFFC00`. First 4 bytes `AA 33 55 CC` = **unprotected**; `55 33 AA CC` = **protected** (debug cannot read flash).
- Removing protection performs a **full chip erase**.
- A second per-block lock (**ACLOCK**, registers FLS_ACLOCK1/2) can hide blocks even when the global flag is off — watch for blocks of 0x00/0xFF in the dump.
- SW-DP IDCODE `0x0BB11477`. Chip UID at `0x1FFFFA10` (12 bytes).
- Official CMSIS pack: **FM33LC0XX_DFP** (FMSH) on keil.arm.com — source of the device header, startup code, SVD and flash algorithm.

**Stock firmware features (for reference):** dose rate + cumulative dose, uSv/uGy, alarms, RTC clock that runs while off, timed measurement, last 10 records stored, Chinese/English menus.

---

## 2. Hard safety rules (never break these)

1. **Never erase, write or mass-erase the chip, and never write option bytes**, unless I explicitly say "erase it" in this conversation after we have a verified backup (or have agreed there can't be one).
2. Never accept/run any "unlock", "unsecure", "remove protection" or "disable read protection" action in any tool. On FM33 this erases the chip.
3. Do not use STM32CubeProgrammer / ST-Link Utility on this chip.
4. The OpenOCD config must have **no flash bank** until Phase 4.
5. HV (~400 V) is present on the board. When you ask me to probe, say where HV is.
6. The ST-Link clone's 3.3 V pin must stay unconnected; the board runs from its battery.

---

## 3. Phase 1 — Connect and check protection

I'll provide `fm33-stlink.cfg` (read-only OpenOCD config: hla_swd, generic cortex_m target, expected-id 0x0bb11477, procs `fm33_protection` and `fm33_dump`). Usage:

```
openocd -f fm33-stlink.cfg -c "init; fm33_protection; shutdown"
openocd -f fm33-stlink.cfg -c "init; fm33_dump; shutdown"     # only if UNPROTECTED
```

Help me:
- Identify pads (GND = continuity to battery −, VDD = 3.3 V, NRST resets the unit when grounded, the other two are SWDIO/SWCLK — swapping them is harmless).
- Troubleshoot connection (speed 200 kHz, connect-under-reset via `reset_config srst_only srst_nogate connect_assert_srst`, or manually holding NRST low while starting OpenOCD).
- Interpret the result. If **UNPROTECTED**: dump 3 times, verify identical SHA-256, keep the backup safe (commit it to a private `backup/` folder, never to a public repo). Also dump the option-byte/info area (0x1FFFF000–0x1FFFFFFF) and RAM.
- If **PROTECTED**: stop and go to Phase 3 (fallback).

---

## 4. Phase 2 — Extract the hardware map from firmware (if dump succeeds)

### 2a. Live register snapshot (stock firmware running)
With the stock firmware running (screen on, then also in each state: idle, clicking, alarm, backlight off, charging), halt and read all FM33 peripheral registers that matter, using addresses from the FM33LC0XX_DFP **SVD**: GPIO (all ports: FCR/mode, alt-function, pull-up, open-drain, DO/DI), RCC/clock config and PLL, SPI, UART/LPUART, ATIM/GPTIM/BSTIM/LPTIM, ADC (channels, config), RTC, EXTI/GPIO interrupt config, FLS option registers, DBG_CR. Resume after each snapshot. Write a script (OpenOCD Tcl or pyOCD Python) that does this and decodes fields by name from the SVD.

### 2b. Static analysis of the dump
- Set up headless Ghidra (or radare2 / `arm-none-eabi-objdump` if quicker) for Cortex-M0, load at 0x0, apply the SVD so peripheral accesses are named.
- Identify: vector table and IRQ handlers; clock init; every GPIO configuration; SPI init and the **ST7789 init command sequence** (extract the exact bytes); display rotation/offsets; backlight control; the **GM pulse input** (pin, edge, EXTI vs timer capture, debounce/dead time); HV enable logic (LM3478 FA/SD) if any; speaker drive (timer/PWM frequency, amp shutdown pin); buttons (pins, active level, long-press logic); **power latch / soft power-off pin**; battery ADC channel and divider math; charger status pins; RTC setup; where settings and the 10 records are stored in flash (address, page size, format); low-power/sleep entry.
- Find the **counts→µSv/h conversion factor** (calibration constant) and any dead-time correction — this tells us the tube sensitivity UNI-T uses.
- Look for strings (menu text, version) to anchor functions.

### 2c. Deliverable: `docs/ut334a-hardware.md`
A complete, sourced hardware map: every MCU pin (QFN-32 pin number, port/pin, function, direction, pull, active level, peripheral + AF), clock tree, peripheral configs, display init bytes, pulse-detection method, calibration factor, flash layout, power behaviour. Mark each fact with its evidence (register snapshot / disassembly address / bench measurement). List anything still unknown and the exact bench measurement that would confirm it. Then stop for my review.

---

## 5. Phase 3 — Fallback if the chip is protected

No dump possible. Before any erase, map the hardware with the stock firmware still running:
- Get the FM33LC0x3N QFN-32 pinout from the datasheet (FM33LC0xx 产品说明书, section 2.1.5 / 2.1.9; debug pins section 6.4.1).
- Guide me through continuity tracing (MCU pin → component) to fill the pin table.
- Scope/logic-analyzer captures: display SPI at power-on (init sequence), GM pulse at the MCU pin, speaker drive, LM3478 FA/SD, button lines, backlight.
- Measure battery divider resistors and HV with a high-impedance divider.
- Produce the same `docs/ut334a-hardware.md`, marked "measured, not from firmware". Only then discuss erasing (needs my explicit OK).

---

## 6. Phase 4 — The Rad Pro FM33 port (after I approve the hardware map)

**How Rad Pro is structured** (verify against the current repo):
- `platform.io/src/` — portable UI, measurements, settings, datalog. Do not modify unless necessary.
- `platform.io/src/peripherals/*.h` — the hardware interface the port must implement.
- `platform.io/src/stm32/stm32_*.c` (~2,700 lines, 15 files: adc, buzzer, cmath, comm, display, events, flash, led, power, pulsesoundenable, rtc, system, tube, vibrator, voice) built on the author's thin register helpers `lib/stm32/stm32-ext.h` — this is what gets an FM33 equivalent.
- Per-device file + header, e.g. `src/stm32/gc03.c` / `gc03.h` (~300 lines + pin defines). **Use the GC-03 port as the main reference**: it drives a 240×320 ST7789 over SPI through `lib/mcu-renderer` callbacks (reset, CS, DC, send8, send16), which are MCU-agnostic.
- `src/sdl/` is the simulator backend — a good minimal example of the interface.

**Plan:**
1. New folder `platform.io/src/fm33/` with `fm33_*.c` for: system (clock, SysTick, IRQs), events, flash (FM33 key sequence, page erase/program), adc (battery), tube (pulse IRQ + timer timestamps; **no HV PWM** — the code already supports this via the `TUBE_HV_PWM` switch; add an HV-enable GPIO only if the firmware shows one), buzzer/sound (speaker via amp; amp shutdown pin), led (if any), power (latch, charge status, sleep), rtc (FM33 RTC is BCD — convert), comm (UART instead of USB; no USB on this chip), display (SPI + backlight), keyboard. Skip: voice, vibrator, EMF, USB.
2. Device files `ut334a.c` / `ut334a.h` modelled on gc03, with pins from `docs/ut334a-hardware.md`.
3. Register definitions/startup from the **FM33LC0XX_DFP** pack (don't hand-write register maps). A thin `fm33-ext.h` helper layer mirroring `stm32-ext.h` style.
4. Build: PlatformIO has no FM33 platform — add a custom board JSON (Cortex-M0, 256K/24K), linker script, startup, `[env:ut334a-fm33lc043n-en]` in `platformio.ini`. Keep all existing envs building.
5. Flashing: pyOCD with the FM33LC0XX_DFP pack, or OpenOCD with a flash algorithm. Only after my explicit "erase it" (see safety rules). Provide a restore script for the stock backup.
6. Bring-up order, one step at a time with a test for each, and I verify on the bench before moving on: power latch + clock → backlight/display → buttons → GM pulse counting (compare CPM against stock firmware on the same background) → speaker → battery/charging → RTC → settings/datalog flash → sleep/current draw → UART comm.
7. Tube sensitivity: use the calibration factor recovered in Phase 2 (or the tube model once identified) and match stock readings.

**Code style:** follow the existing Rad Pro conventions; minimal changes to shared code; keep FM33 code behind its own defines so upstreaming is possible.

---

## 7. How to work with me
- Before each phase, give a short plan; then do it.
- When you need a bench measurement, tell me exactly where to probe and what result means what.
- Keep a running `docs/ut334a-notes.md` log of findings and decisions.
- Never assume a pin or value you haven't seen in the dump, a register snapshot or my measurement — write "unknown" instead.

**Start with Phase 1.**
