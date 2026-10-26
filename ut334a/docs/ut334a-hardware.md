# UNI-T UT334A: hardware map (from the stock firmware)

Status: **Complete for Phase 4, 2026-10-25** (bench-confirmed where marked [B]; the stock unit counts normally). Sources: static analysis of the stock flash dump
(`backup/ut334a_stock_1.bin`, SHA-256 `6815991f…d939`), Ghidra 12.2 decompilation (`analysis/ghidra/`),
emulation of firmware routines (`tools/emu.py`, Unicorn), the FM33LC0xx datasheet V1.9 and the FMSH
FM33LC0XX_DFP 3.0.4 SVD. Live register snapshots were not possible (the stock firmware parks the SWD pins,
see notes), so every "register value" below comes from emulating the firmware's own init code.

Evidence tags: **[S 0xNNNN]** static (code address), **[E fn]** emulation of that function,
**[B]** bench measurement, **[DS §x]** datasheet. Items marked **UNCONFIRMED** need a bench check (see last section).

**HV warning for all probing:** the ~400 V section is the LM3478 boost (SOIC-8), the 1.5 mH inductor ("152"),
the 6-diode multiplier ladder and both tube ends/clips. Probe only the MCU side unless stated.

---

## 1. MCU and clocks

| Item | Value | Evidence |
|---|---|---|
| MCU | FM33LC043N, QFN-32, Cortex-M0, 256 KB flash, 24 KB RAM | marking, CPUID 0x410CC300 [B] |
| SYSCLK | RCHF 8 MHz → PLL (REFPRSC=3 → 1 MHz ref, DB=0x3F → ×64) = **64 MHz**, SYSCLKSEL=2 | [E 0x6DD8(0x40)], SystemCoreClock var = 64000000 |
| Flash wait states | FLS_RDCR.WAIT = 2 at 64 MHz, 0 in low-speed mode | [E 0x6DD8] |
| Low-speed clock | 32.768 kHz crystal on **PD9/PD10** (XT32KO/XT32KI), RCC_XTLFCR.EN=5, LSCLKSEL=0xAA (XTLF) | [E 0xD064], [S 0x6D98] |
| Low-speed mode | `0x6DD8(0)`: back to RCHF, PLL off, 0 wait states (used before power-off) | [S 0x6DD8] |
| Watchdog | IWDT served in main loop (`IWDT_SERV = 0x12345A5A`); DBG_CR=3 stops IWDT/WWDT while halted | [S 0xDC68], [S 0xC724] |
| Option bytes vs IWDT | OPTBYTES high half = 0xFFFF → IWDTSLP = 0xF ≠ 0xA, so **the IWDT keeps counting in Sleep/DeepSleep** (freeze not allowed) | [B dump], [DS 7.4.2, 9.5] |
| SVD errata | FM33LC0XX.SVD (DFP 3.0.4) puts the VREF registers 0x0C too high; header (VREF_CR 0x4001A80C) is right, matching the stock firmware | [DS 5.2], [S 0x310C] |
| Boot | Reset → SystemInit 0xC6FC (FMSH template) → Keil `__main` → `main` 0xDC68 | [S 0x17C] |

## 2. Pin map (all 32 package pins)

QFN-32 numbering from datasheet fig. 2-4 (pin 1 = PD8, counter-clockwise); pads 1–3 checked by continuity [B].

| QFN | Port | Function in stock firmware | Dir / mode | Pull | Active | Evidence |
|---|---|---|---|---|---|---|
| 1 | PD8 | SWDIO. **Stock FW parks it: output, driven low** (no other use) | out PP | – | – | [S 0x6BBA, 0x6BD0], SWD pad 2 [B] |
| 2 | NRST | reset, SWD pad 1 | – | – | low | [B] |
| 3 | PA13 | output, driven low at init; never used. Bench: no tracks | out PP | – | – | [S 0x6BE8], [B] |
| 4 | PA14 | output; never used. Bench: no tracks | out PP | – | – | [S 0x6BE8], [B] |
| 5 | PA15 | **Left key** (firmware flag 0x20000E2B) | in | pull-up | low | [S 0x2484], [B] |
| 6 | PA8 | **GM pulse input → COMP2_INP1** (analog) | analog | – | rising above 0.6 V | [E 0x6A4C] |
| 7 | PA9 | **External power (USB) detect**, also EXTI wake-up from DeepSleep (EXTI2, ASEL=PA9, filter on). Bench: divider from USB 5 V (after a series resistor marked "560"), same net as the 4057 charger VIN | in | none | high = powered | [S 0xC974], [E 0x4F68], [B] |
| 8 | PA10 | **OK / Power key**: short = select, hold ~3 s = power on/off | in | pull-up | low | [S 0x2484], [B] |
| 9 | PB2 | **Backlight PWM**, ATIM_CH1N | AF | – | – | [E 0x69C4], [S 0x58B8] |
| 10 | PB3 | **LCD CS** | out PP | – | low | [S 0x6208, 0x61D4] |
| 11 | PB8 | **LCD D/C** (low = command, high = data) | out PP | – | – | [S 0x6208, 0x61D4] |
| 12 | PB9 | **LCD SCL**, SPI1_SCK | AF | – | – | [S 0xBAF2] |
| 13 | PB10 | **Speaker tone PWM**, GPTIM0_CH1 → coupling cap → 20 kΩ → 8002A input | AF | – | – | [E 0x6C6C], [B] |
| 14 | PB11 | **LCD SDA**, SPI1_MOSI (no MISO used) | AF | – | – | [S 0xBAF2] |
| 15 | PB13 | **LCD RESET** | out PP | – | low | [S 0x594C] |
| 16 | PB14 | **LCD power switch**: gate of a P-MOSFET (marked "39K") from VBAT to the LCD supply | out PP | – | low = LCD on | [S 0x594C, 0xA90C, 0x6C56], [B] |
| 17 | PC2 | **Bi-colour LED, side B**. Bench: PC2 and PC3 drive the two ends of an anti-parallel LED pair. PC2 high (PC3 low) = colour B, used for alarm beeps | out PP | – | high | [S 0xB37C], [B] |
| 18 | PC3 | **Bi-colour LED, side A**: PC3 high (PC2 low) = colour A (green, bench 2026-10-26); flashes ~10 ms per GM pulse, on while charging | out PP | – | high | [S 0x2750, 0x6254, 0x26F4], [B] |
| 19 | PC4 | **HV-rail switch**: N-MOSFET ("C") → gate of P-MOSFET ("39K") VBAT → **HVbatt** (LM3478 supply and battery divider) | out PP | – | high = on | [S 0x594C, 0xA904], [B] |
| 20 | PC5 | **HV converter run/stop**: 200 Ω → N-MOSFET ("C") gate; drain → 160 kΩ → LM3478 pin 7 (FA/SD), drain also 47 kΩ to HVbatt. PC5 high = FA/SD sees 160 kΩ to GND = converter runs; PC5 low = FA/SD pulled up = shutdown. Firmware bursts: 1 tick in 5 (4.9 ms every 24.4 ms, 41 Hz, 20 %) + one burst after every GM pulse | out PP | – | high = HV on | [S 0x2788, 0x627E–0x629E], [B] |
| 21 | PC8 | **Battery sense**, ADC_IN9, divider (ratio 0.55) on **HVbatt**, so only valid while PC4 is on | analog | – | – | [E 0x6950], [S 0x5658], [B] |
| 22 | PC9 | output, driven low; never used afterwards | out PP | – | – | [S 0x6C10] |
| 23 | PC10 | output, driven low; never used afterwards | out PP | – | – | [S 0x6C10] |
| 24 | PD9 | XT32KO (32.768 kHz crystal) | analog | – | – | [S 0x6DB2] |
| 25 | PD10 | XT32KI | analog | – | – | [S 0x6DCC] |
| 26 | VDD15 | 1.5 V core LDO output (cap) | – | – | – | [DS 2.1.4] |
| 27 | VSS | ground | – | – | – | [DS] |
| 28 | VDD | 3.0 V supply (SWD pad 4 measured 3.0 V) | – | – | – | [B] |
| 29 | PD11 | **Right key** (firmware flag 0x20000E2A), bench-confirmed | in | pull-up | low | [S 0x2484], [B] |
| 30 | PD0 | output, driven low; never used afterwards | out PP | – | – | [S 0x6BBA] |
| 31 | PD1 | output, driven low; never used afterwards | out PP | – | – | [S 0x6BBA] |
| 32 | PD7 | SWCLK. **Stock FW parks it: output, driven low** | out PP | – | – | [S 0x6BBA], SWD pad 3 [B] |

Notes:
- "never used afterwards": no later set/reset/read found in the image. PA13/PA14 are UART0/LPUART0 capable and
  PC9/PC10 are SPI2 pins, so they may go to test pads or nothing. Worth a continuity check; a free UART on
  PA13/PA14 would be the natural Rad Pro comm port (USB is not available on the "N" part).
- Port-pin numbers in hex in the firmware: GPIO helper `FL_GPIO_Init` at 0x4128, struct {pins, mode(0 in,1 out,2 AF,3 analog), od, pull, ...}.
- In power-off (0xA884) the firmware also switches PC8, PB9, PB11, PA8 to open-drain outputs and drives
  PC4, PB13, PB8, PB3, PC5, PC2, PC3, PB9, PB11 low and PB14 high before DeepSleep.

## 3. Display

| Item | Value | Evidence |
|---|---|---|
| Controller | ST7789-family (command set matches) | [S 0x5960] |
| Resolution | **240 × 320**, portrait (full-screen fill `lcd_fill_rect(0,0,0xF0,0x140)`) | [S 0xAA3C, 0x284C] |
| Bus | SPI1 master, 8-bit for commands; 16-bit frames for CASET/RASET data and pixels; SCK=PB9, MOSI=PB11 | [S 0xBA90, 0x5C04] |
| CS / DC / RST | PB3 / PB8 / PB13 (GPIO) | [S 0x594C, 0x6208] |
| Pixel transfer | DMA channel 6 → SPI1 (`0xBB04`) | [S 0x6B34, 0xBB04] |
| Backlight | PWM on PB2 (ATIM CH1N): PSC=1, ARR=9999 → **3.2 kHz**, CCR1=5000 (50 %) at init. **Lit while PB2 is high** (bench: PB2 low as GPIO = off). With only CC1NE enabled, CH1N follows OC1REF, so PWM mode 1 gives on-time = CCR1 (mode 2 inverts: CCR1 0 = brightest). Stock menu mapping not traced | [E 0x6A1C, 0x69C4], [B] |
| Sleep/wake | sleep: ATIM off (`0x5EB8`); wake: SLPOUT (0x11) + ATIM on (`0x58B8`) | [S] |
| Colour format | RGB565 (COLMOD 0x05) | [S 0x59C6] |
| Init order | **COLMOD must come after SLPOUT**: this panel ignores COLMOD while in sleep and stays at the 18-bit default (text repeated 3×, striped greys). Stock order: SLPOUT, 120 ms, MADCTL, COLMOD 0x05 | [B 2026-10-25] |

**Power-up and init sequence** (`0x594C` → `0x5960`, exact):
```
PC4=1; PB8=0; PB3=1; PB14=0; delay 1 ms
PB3=0; delay 1; PB13=0; delay 1; PB13=1; PB3=1; delay 1
spi1_init()
11                       SLPOUT, delay 120 ms
36 00                    MADCTL
3A 05                    COLMOD RGB565
38                       IDMOFF
3C                       (sent with no data)
51 00                    WRDISBV
E7 00                    SPI2EN
B2 0C 0C 00 33 33        PORCTRL
B7 35                    GCTRL
BB 32                    VCOMS
C2 01                    VDVVRHEN
C3 15                    VRHS
C4 20                    VDVS
C6 0F                    FRCTRL2
D0 A4 A1                 PWCTRL1
E0 D0 08 0E 09 09 05 31 33 48 17 14 15 31 34   PVGAMCTRL
E1 D0 08 0E 09 09 15 31 33 48 17 14 15 31 34   NVGAMCTRL
21                       INVON
29                       DISPON
```
Window/write: `2A x0 x1` (16-bit), `2B y0 y1` (16-bit), `2C` then pixels [S 0x5C04]. No column/row offset is applied.

## 4. GM tube pulse detection

| Item | Value | Evidence |
|---|---|---|
| Input | PA8 → **COMP2** positive input (INP1); negative input **VREF/2 = 0.6 V**; polarity non-inverted; digital filter on (CMP2DF) | [E 0x6A4C], [DS §17.5] |
| Interrupt | COMPx IRQ 25, priority 2; CMP2IE set in board_init | [S 0x6AFA, 0x6D48] |
| Pulse shape at PA8 [B] | baseline ≈ 0.04–0.08 V, sharp rise to **≈ 2.1 V** (1.9–2.2 V), decay to baseline in **≈ 60 µs**; above the 0.6 V threshold for ≈ 45–50 µs. Scope 10 µs/div, trigger 1.76 V rising (2026-10-25) | [B] |
| Counting | ISR `0x2750`: if CMP2 flag and `CMP2O`=1 → `pulse_count(0x20000E34)++`; no software dead time (only the comparator filter) | [S 0x2750] |
| Per-pulse effects | PC5 high; PC3 high unless 0x20000000 (off-state) set; if "particle sound" on (0x20000DAA==8) and not alarming: GPTIM0 CCR1=0x19C8, start tone | [S 0x2750] |
| Pulse effect end | LPTIM tick ISR: after 2 ticks (≈9.8 ms) stops GPTIM0 and clears PC3 | [S 0x6254] |

There is no MCU PWM for the HV supply (no timer drives a pin other than PB2 backlight and PB10 tone).
**HV supply control (bench-confirmed topology):** PC4 switches VBAT onto the HVbatt rail; PC5 gates the LM3478 through its FA/SD pin (high = run with R_FA = 160 kΩ, low = shutdown). The stock firmware regulates nothing in software: it simply runs the boost converter in bursts, 1 LPTIM tick in 5 (≈20 % duty at 41 Hz) plus a burst after each GM pulse (until the next tick). PC5 has no timer alternate function, so this burst pattern must be generated in software (tick interrupt), as the stock firmware does.

## 5. Timebase and measurement

| Item | Value | Evidence |
|---|---|---|
| Tick | LPTIM32 clocked from LSCLK (32.768 kHz), ARR=160 → **204.8 Hz (4.883 ms)**, IRQ 20 priority 0 | [E 0x6D18], [S 0x6254] |
| Measurement step | `measurement_tick` (0x5094) every 20 ticks (97.7 ms) from main | [S 0xDC68] |
| Window | 20 steps = **1.953 s**, the firmware treats it as **2 s** (seconds counter +2) → readings ~2.4 % low | [E 0x5094] |
| Real-time rate | moving sum of last 10 windows (≈20 s); fast response: if a new window differs from slot 5 by ≥12 counts, older slots are overwritten | [S 0x5094] |
| Formula | **rate[µSv/h] = CPM × k / 60**, CPM from the 10-window sum | [E 0x5094] |
| Average rate | total counts / elapsed minutes × k / 60 (cap 99999) | [E 0x5094] |
| Total dose | average rate × elapsed hours (cap 1e7) | [E 0x5094] |

**Calibration** (`update_calibration_factor` 0x5008, verified by emulation):

| Real-time rate | k | Tube sensitivity implied (C1=C2=1) |
|---|---|---|
| < 0.8 µSv/h | 0.9 × C1 | 66.7 CPM per µSv/h |
| 0.8 – 20 µSv/h | 1.332 × C1 | 45.0 CPM per µSv/h |
| ≥ 20 µSv/h | 1.54 × C2 | 39.0 CPM per µSv/h |

- C1 at 0x20000DB8, C2 at 0x20000DBC (floats), **factory default 1.0 / 1.0** [E C runtime init], editable
  in the hidden data menu ("Calibration Factor C1=/C2=", `0x284C`).
- The rising factor at higher rates acts as a coarse dead-time correction; there is no explicit dead-time formula.
- Taking the window error into account, true sensitivity is ~2.4 % higher than the table (e.g. 46.1 CPM per µSv/h mid-range).

## 6. Sound, alarm, LED

| Item | Value | Evidence |
|---|---|---|
| Tone | GPTIM0 CH1 on PB10, PSC=2, ARR=13399 → **1.592 kHz** at 64 MHz; volume = PWM duty (CCR1) | [E 0x6CCC, 0x6C6C] |
| Amplifier | 8002A, shutdown pin tied to GND (always enabled) per bench; input from PB10 still to confirm | [B] |
| Click | CCR1=0x19C8 (≈49 % duty), ~9.8 ms per pulse | [S 0x2750, 0x6254] |
| Alarm | pattern stepped by counter 0x20000D30; tone + PC2 high in "on" phases; CCR1 = `vol×0x2E8 − (9−vol)×0x50`, vol 0–9 (0x20000DA8) | [S 0xB37C] |
| LED | Bi-colour (anti-parallel pair) between PC3 and PC2: colour A = PC3 high (pulse flash, charging), colour B = PC2 high (alarm) | [S], [B] |

## 7. Power, battery, charging

| Item | Value | Evidence |
|---|---|---|
| Battery sense | PC8/ADC_IN9 + internal VREF channel; `V[mV] = ch × 3000 × VREF_CAL / (vref × 4095) / 0.55` | [S 0x5658, 0x55E0, 0x56DC] |
| VREF_CAL | 0x1FFFFB08 = **1645** on this chip (complement valid) | [B dump] |
| Divider | firmware assumes **0.55**; bench: 2.1970 V at the divider output with 4.040 V at the battery = **0.5438** (resistors not measurable in circuit). So the stock firmware reads the battery ~1.1 % high | [S 0x56B4], [B] |
| Sampling | every 5 windows (~10 s), 8-sample average (0x2000123C) | [S 0x5398–0x53E8] |
| Gauge | ≥ 4020 mV → 20/20; else `20 − (4020 − V)/28.5` steps | [S 0x544E–0x548E] |
| Low battery | < 3350 mV for 10 samples, no external power → power off | [S 0x53EE–0x5430] |
| Charging indicator | PA9 high **and** battery < 4100 mV → "Charging", PC3 on; else "Battery Full". Voltage-based: **no charger status pin is read** | [S 0x26F4, 0xAA3C] |
| External power | PA9 high (debounced 10 ms) powers the unit up and keeps it awake | [S 0xC974] |
| Charger | 6-pin 4057 (TP4057-class) Li-ion charger, VIN on USB 5 V; its status pins are not read by the MCU | [B], [S] |
| Power key | PA10 held ≥ 600 ticks (2.9 s) → `0x7780` (power toggle) | [S 0x2484] |
| Power off | `0xA884`: park pins (see §2; PC8, PA8 and PB9/PB11 as open-drain outputs), PDR on + **BOR off** (BORCR.OFF_BOR_1P2, 1.2 µA typ. per datasheet), IWDT FREEZE set (ineffective: option bytes forbid it), VREF CR.EN cleared, ADC, COMP2, LPTIM, DMA off, **RTC SEC_IE cleared**, PMU_CR = Sleep+SLPDP (DeepSleep), `WFI`. So stock does not wake every second: wake by EXTI on PA9 (USB), key, or (unconfirmed) the IWDT reset with the off flag kept in RAM; the main loop polls PA10 (hold ~3 s → 0x7780 toggles the off flag 0x20000000). No physical power switch [B] | [S 0xA884, 0x7780], [E 0x4F68], [DS 8.8.2, 3.4.2.5] |
| Auto power-off / screen sleep | settings in hours (×7200 half-seconds) and minutes (×120) | [S 0xDC68] |

There is no separate power-latch pin: "off" is MCU DeepSleep with RAM and RTC retained. PC4 switches the HVbatt rail (HV converter + battery divider),
PB14 the LCD supply (both P-MOSFET high-side switches).

## 8. RTC, settings and records storage

| Item | Value | Evidence |
|---|---|---|
| RTC | FM33 RTC on the 32.768 kHz crystal; init `0x8330`, IRQ 2 handler `0x7830`; default date written if the RTC is invalid | [S 0x8330] |
| Settings and 10 records | **kept in RAM only.** The firmware never programs or erases flash: the only flash-controller register it touches is FLS_RDCR (wait states). Data survive "off" (DeepSleep) but are lost if the battery is disconnected | [S, whole-image search of FLS refs] |
| Flash key state machine (for our own writes) | After an erase/program, **one** KEY write closes it; **KEY = 0 in the protected state is a key error** (KEYERR, KEYSTA 100) that blocks erase/program until a power-on reset or NRST (SYSRESETREQ does not clear it). Status reads 0 while the flash clocks (PCLKCR2.FLASH_PCE, OPCCR2.FLASHCKE) are off | [B 2026-10-26] |
| Defaults | from the C runtime initialised data (`analysis/ram_initial.bin`, emulated) | [E 0xC0] |

## 9. Other

- UI: two navigation keys (PD11 "A", PA15 "B") + OK/Power (PA10); key repeat after 1.2 s, accelerating.
- Fonts in flash: 8×16 at 0xE548, large digits at 0xF96E (216 bytes/glyph), 16×16 Chinese at 0xEB38 (GB2312 code + 32 bytes).
- Unused peripherals: no UART, I2C, USB, LCD-controller, flash programming.

## 10. Still unknown → bench measurements

Resolved on the bench 2026-10-25: PC5 → HV run/stop MOSFET; PC4 → HVbatt switch; PB14 → LCD supply switch;
PC2/PC3 → bi-colour LED; right key → PD11 (QFN 29); battery divider ratio 0.5438 (measured); GM pulse at PA8 ≈ 2.1 V peak, ≈ 60 µs (an earlier "no pulse / constant 0.5 V" was a solder bridge between QFN 5 and 6, since removed); HV ≈ 314 V at the multiplier with a 10 MΩ meter (≈ 425 V unloaded, estimated); 8002A SD → GND, input ← PB10 via coupling cap + 20 kΩ; PA9 → USB 5 V divider;
PA13/PA14 → no tracks; keys: PA15 left, PA10 OK/power.

| # | Question | Measurement | What the result means |
|---|---|---|---|
| 3 | LCD backlight supply | **Polarity resolved (bench 2026-10-26): lit while PB2 high.** Supply source (PB14 rail, VBAT or 3.0 V) still unknown | Off-state current |
| 5 | HV burst behaviour | (optional, HV probe only) tube voltage ripple with the stock burst pattern | Baseline for Rad Pro HV profile |
| 6 | Pads 2/3 | Continuity from pads 2/3 to anything other than QFN 1/32 | Confirms SWD pins are free for Rad Pro |
| 7 | Tube model | Re-read the glass marking with magnification | Cross-check with 45 CPM per µSv/h |
