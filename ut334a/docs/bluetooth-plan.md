# HC-05 Bluetooth serial: plan (2026-11-01)

**DROPPED 2026-11-01** (user): with the HC-05 powered from the unit (UART not connected), every message gave
false counts and alarms; 100 µF only partly helped. See `docs/ut334a-notes.md`. Kept for reference only.

Optional add-on after step 5. Nothing below was flashed.
Goal: Rad Pro's serial protocol (GeigerLog, `radpro-tool`, Android "Serial Bluetooth Terminal") over Bluetooth,
with the module switched on and off in software. Decisions so far (notes 2026-11-01):
- Module: **HC-05** (classic SPP, makerselectronics.com, 195 EGP). BLE modules (BT-05, HM-10) give no COM port
  on Windows.
- UART: **UART5, PD0 RX / PD1 TX (QFN 30/31)**, the only UART with wires. The FTDI comes off: no wired serial
  afterwards (flashing stays on SWD).
- Control: a **dedicated MCU pin** (not PC4/HVbatt). Module powered only while the unit is on and Bluetooth is
  enabled; never while the unit is off.

## Hardware edits

### Parts (makerselectronics.com, in stock 2026-11-01)
| Part | Use |
|---|---|
| HC-05 breakout | Module. VCC 3.6–6 V (on-board regulator), 3.3 V logic, ≈ 50 mA spec, 9600 baud default |
| AO3401 (P-MOSFET, SOT-23) | High-side switch, battery → HC-05 VCC |
| AO3400 (N-MOSFET, SOT-23) | Level shifter: fully on from the MCU's 3.0 V |
| 2 × 100 kΩ | AO3401 gate pull-up to VBAT; AO3400 gate pull-down |
| 3 × 1 kΩ | AO3400 gate series; UART lines (both directions) |

A P-MOSFET straight from the 3.0 V pin does not work: with VBAT at 4.2 V, a high pin leaves Vgs = −1.2 V,
about the AO3401 threshold, so it never switches fully off.

### Circuit
```
VBAT ──┬──────────── S
       │                AO3401 (P)
     100k          G ──┤
       │           │    D ──── HC-05 VCC
       └───────────┤
                   D
              AO3400 (N)
 PC10/QFN23 ─1k─ G
              │ S ── GND
            100k
              │
             GND

HC-05 TXD ──1k── QFN 30 (PD0, UART5 RX)
QFN 31 (PD1, UART5 TX) ──1k── HC-05 RXD
HC-05 GND ── board GND        HC-05 STATE, EN: not connected
```
- Pin high = module on. Pin low, floating or the MCU in reset = off (both 100 kΩ hold it off).
- While on, the gate pull-up draws VBAT / 100 kΩ ≈ 42 µA; off-state current unchanged.
- PD0 is not 5 V tolerant (only PB12 is, notes 2026-10-25) and VDD is 3.0 V: the 1 kΩ in the HC-05 TXD line
  limits the clamp current from its 3.3 V high level (≈ 0.3 mA).
- VBAT: where to tap it on the board (battery + pad, charger output) is unknown: user to choose. Not from
  HVbatt (its P-MOSFET also feeds the battery divider) and not from the LCD supply.

### Interference and placement (2026-11-01)
GM tube itself is not RF-sensitive (it counts ionizing events only); the risks are in the electronics and the
mechanics. Off (Data mode off or unit off) the module is unpowered and has no effect.
- **Supply noise (most likely issue):** HC-05 current bursts (≈ 30–40 mA pairing, TX peaks) dip VBAT, which also
  feeds HVbatt (LM3478 + battery divider). Possible effects: small HV shift (tube plateau, expected harmless),
  low battery reading (earlier low-battery shutdown at < 3350 mV), ground bounce near PA8 (pulse ≈ 2.1 V vs
  0.6 V threshold: wide margin).
- **RF pickup:** class 2, ≈ 2.5 mW, frequency hopping: small risk, not zero if the antenna sits on the tube or
  its anode network.
- **HV → module:** multiplier is high impedance (µA), module has its own regulator, 1 kΩ UART resistors damp
  LM3478 switching noise. Real risk is mechanical: contact or arcing.
- **Placement:** module and wires away from the LM3478, 1.5 mH inductor, diode ladder and both tube ends;
  Kapton on the module's back, insulated wire; antenna end (PCB trace antenna) pointing away from the tube and
  HV section, not covered by the battery or metal.
- **Decoupling:** whether the breakout's own caps are enough is **unknown**. If T6/T7/T7a show a problem, first
  fix: 10 µF + 100 nF at the module VCC (no large cap: AO3401 inrush would dip VBAT itself).

### Control pin (open)
**PC10 (QFN 23)**, chosen after H2 (2026-11-01): QFN 23 free, QFN 22 (PC9) hard-wired to GND (unusable; the
port never touches it, keep it that way). PA13/PA14 (QFN 3/4): not solderable (user).
Fallback only: SWD pins (would mean the manual NRST hold for every flash).

### Before fitting: HC-05 setup (once, on the PC with the FTDI, 3.3 V logic)
1. EN/KEY high while powering the module → AT mode (38400 8N1, CR LF), LED blinks slowly.
2. `AT` → `OK`; `AT+UART=115200,0,0`; optionally `AT+NAME=UT334A`, `AT+PSWD="xxxx"` (syntax depends on the
   module's firmware: `AT+VERSION?` first).
3. Power off, EN open, power on: data mode at 115200 (LED blinks fast until paired).

## Firmware edits (fork, `ut334a` branch)
1. `ut334a.h`: `BT_POWER_PORT` / `BT_POWER_PIN` = GPIOC / 10 (PC10, QFN 23).
2. `ut334a.c` init: the pin as push-pull output, low (module off from reset).
3. `platformio.ini` + `tools/platformio.ini.base`, UT334A section: `-DDATA_MODE`. That enables Rad Pro's
   Settings → Power → Data mode (stored bit `settings.dataMode`, default off; string exists in all languages).
4. `peripherals/comm.c` `updateComm()`: with `POWER_OFF_SLEEP`, `commShouldBeOpen = settings.dataMode &&
   isPoweredOn()`. `updateComm()` also runs while the UT334A is off (main loop keeps running), and stock Rad
   Pro would keep the port open when off. Shared code: list it in the fork's `ut334a/README.md`.
5. `fm33_comm.c`: `openComm()` switches the module on, then sets PD0/PD1 to UART; `closeComm()` sets the pins
   analog, then switches the module off (the MCU never drives an unpowered module).
6. `fm33_power.c` `sleepWhilePoweredOff()`: `closeComm()` before `parkDisplay()`, so the module is off in
   DeepSleep even if the off event and the sleep happen in the same loop pass.
7. Docs: hardware map (new pin, circuit), install guide (serial section → Bluetooth), notes.
8. Build all 30 languages; emulator: pin low at boot; Data mode on (poke) → pin high, UART reply over
   `EMU_UART_RX`; power off → UART closed, pin low before DeepSleep; Data mode off → pin low.

### Open decisions (user)
- Data mode after power-on: remembered (Rad Pro default), or always off at power-on?
- ~~Low battery~~ **Decided 2026-11-01:** keep the breakout regulator (H1: drops out at ≈ 3.5 V while
  connected, regulator loses ≈ 0.4 V). Firmware switches Data mode off below ≈ 3.55 V on battery (no USB);
  it stays off until the user turns it on again. Exact threshold set in the firmware step, checked in T6.
- Menu label: keep "Data mode" or rename to "Bluetooth" (UT334A only, 30 languages)?
- Data mode with USB plugged in while on: same as on battery (assumed, to confirm).

## Tests (one state at a time, each held until reported)
Before the firmware:
- **H1** HC-05 alone at ≈ 3.5 V (the unit's battery today, or a supply): pairs and passes data (loopback
  TXD–RXD). Shows whether it works below its 3.6 V spec.
  Result 2026-11-01: starts from 3.4 V, dead below (LED dark).
- **H2** Continuity (battery and USB out; keep away from the diode multiplier, tube ends, LM3478 / 1.5 mH
  inductor): QFN 22 and QFN 23 each against GND, VDD (SWD pad 4), neighbours QFN 21 / 23 / 24, nearby vias.
  Open = free. Short to a neighbour = solder bridge. Pad or via = possible solder point.
- **H3** After wiring, old firmware: HC-05 VCC = 0 V with the unit on and off (pull-down holds it off).

After the flash:
- **T1** Unit on, Data mode off: HC-05 VCC 0 V, module LED dark.
- **T2** Data mode on: VCC ≈ battery (within ≈ 10 mV), LED blinks fast.
- **T3** Pair with the PC; "Outgoing" COM port; terminal 115200: `GET deviceId` → `OK UNI-T UT334A;...`.
- **T4** `radpro-tool.py --port COMx --download-datalog datalog.csv`; GeigerLog live logging.
- **T5** Android, Serial Bluetooth Terminal: `GET tubeRate`, `GET deviceBatteryVoltage`.
- **T6** Battery: DMM across the battery vs Settings → Statistics → Battery, module on and connected, with
  data streaming (e.g. GeigerLog live), to see how far the reading drops while it transmits.
- **T7** Background count rate, 10 min with the module **paired and streaming** vs 10 min off (HV / GM
  front-end noise). Streaming = worst case (most current, most TX).
- **T7a** (optional, scope) PA8 (QFN 6) while streaming, no source near, trigger 0.6 V rising: anything other
  than a clean ≈ 2.1 V / ≈ 60 µs pulse is pickup.
- **T8** Current from the battery while on: Data mode off vs on (and connected).
- **T9** Power off with Data mode on: HC-05 VCC 0 V; off-state current back to 3–8 µA.
- **T10** Power on again: Data mode state as decided above.
- **T11** Auto shutdown (1 h) and low-battery shutdown with Data mode on: module off afterwards.
- **T12** USB plug-in while off: charging screen, module stays off.
