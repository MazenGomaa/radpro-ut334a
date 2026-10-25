# Back up your UT334A before installing Rad Pro

The stock UNI-T firmware is not distributed with this port. **Dump your own unit first.** Without a backup,
there is no way back to the stock firmware if Rad Pro misbehaves on your unit or a flash goes wrong.

This takes about 15 minutes and only reads the chip. Nothing is erased or written.

## What you need
- An ST-Link V2 (clones work) and OpenOCD 0.12 or newer. PlatformIO ships one in
  `~/.platformio/packages/tool-openocd`.
- `fm33-stlink.cfg` from this folder. It is read-only: it has no flash bank, so OpenOCD cannot erase or write.
- A charged UT334A battery, a fine soldering iron (or pogo pins), and tweezers or a push-button.

## Safety
- **High voltage:** about 400 V on the HV section (LM3478 converter, 1.5 mH inductor, diode multiplier, tube
  ends). The SWD pads are next to the MCU, away from it, but do not touch the HV parts while the unit is open.
- **Never connect the ST-Link 3.3 V pin.** The board runs from its own battery (VDD is 3.0 V).
- **Never accept "unlock", "unsecure" or "remove read protection"** in any tool. On the FM33LC this erases the whole
  chip, including the stock firmware you are trying to save.
- Do not use STM32CubeProgrammer or ST-Link Utility on this chip.

## 1. Wire up
The row of 5 pads left of the MCU (square pad = pad 1):

| ST-Link | Pad | MCU (FM33LC043N, QFN-32) |
|---|---|---|
| GND | 5 | GND |
| SWDIO | 2 | pin 1, PD8 |
| SWCLK | 3 | pin 32, PD7 |
| RST | leave disconnected | – |
| 3.3 V | **never connect** | – |
| – | 1 = NRST (pin 2) | you short this to GND by hand |
| – | 4 = VDD (3.0 V) | – |

QFN-32 is fine pitch: inspect for solder bridges afterwards.

## 2. Why you must hold NRST
The stock firmware turns the SWD pins into outputs right after boot, so the debugger can only connect while the
chip is held in reset. The config then stops the CPU at its very first instruction, before the firmware runs, and
reads the flash. The core's ID (CPUID 0x410CC300) is rejected by OpenOCD's normal Cortex-M target, which is why
the config uses plain memory access (`mem_ap`).

## 3. Dump
1. Short pad 1 (NRST) to GND with tweezers or a push-button and keep holding it.
2. Within a few seconds, run (from the folder containing `fm33-stlink.cfg`):
   ```
   openocd -s <openocd>/openocd/scripts -f fm33-stlink.cfg -c "init; fm33_session"
   ```
   If it fails to connect, just run it again while still holding NRST. A retry loop helps (bash):
   ```bash
   O=~/.platformio/packages/tool-openocd; end=$((SECONDS+180))
   while [ $SECONDS -lt $end ]; do
     out=$(timeout 150 $O/bin/openocd -s $O/openocd/scripts -f fm33-stlink.cfg -c "init; fm33_session" 2>&1)
     echo "$out" | grep -q DPIDR && { echo "$out"; break; }
   done
   ```
3. Hold NRST for about 10 s after starting, then **release it**. The script prints `ARMED`, then `HALTED at reset vector`.
4. The script checks protection and dumps only if the chip is unprotected:
   - `UNPROTECTED`: it writes `ut334a_stock_1.bin`, `_2.bin`, `_3.bin` (256 KB flash, read three times),
     `ut334a_info_1FFFFA00-1FFFFFFF.bin` (option bytes, chip UID, calibration) and `ut334a_ram.bin`.
   - `PROTECTED`: **stop.** Your unit cannot be backed up over SWD. Unlocking erases the chip, so installing
     Rad Pro would be a one-way trip.
   - Anything else: stop and open an issue with the printed bytes.
5. The script releases the CPU and the unit boots normally.

## 4. Verify
```
sha256sum ut334a_stock_*.bin
```
All three must be identical. If they differ, the link was unreliable: dump again. The unit used for this port
had `6815991f2185751a0f31808c5e766a3113ab04f9bfa443a6e09d5304b611d939`; a different hash may just be a different
stock firmware version.

Also check that the files are not all `0x00` or all `0xFF` (that means the core was not halted when reading).

## 5. Keep it safe
Store the backup in at least two places (for example the PC and a USB stick or private cloud storage).
Do not publish it: it is UNI-T's copyrighted firmware.

## Restoring
Same wiring and NRST hold. `tools/fm33_flash.sh` flashes an image over SWD (page erase of the whole main
flash, program, full readback compare, reset). In the port's workspace `restore` uses `backup/ut334a_stock_1.bin`
and checks its SHA-256 first; with your own dump, run `tools/fm33_flash.sh flash <your dump>.bin --erase-it`.
The tool has not yet been run on real hardware (see `docs/ut334a-notes.md`).

## If the unit stays dark after a failed session
Debug settings survive a normal reset. Hold NRST, run
`openocd -s <openocd>/openocd/scripts -f fm33-stlink.cfg -c "init; source tools/fm33_clear_debug.tcl"`,
then release NRST. Or disconnect the battery for 5 s.
