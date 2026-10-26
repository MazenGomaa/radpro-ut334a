# Install Rad Pro on the UNI-T UT334A

The port is **experimental**: see "What works" in the fork's README before you start. The stock UNI-T firmware is
not distributed with this port, so **back up your own unit first** (step 3). Without a backup there is no way back
to the stock firmware if Rad Pro misbehaves on your unit or a flash goes wrong.

Overview: wire up the SWD pads (15 min), dump the stock firmware (read-only), build Rad Pro, flash it (≈ 6 min).
There is no USB data connection on this unit: installing and updating always go through SWD.

## What you need
- An ST-Link V2 (clones work) and OpenOCD 0.12 or newer. PlatformIO ships one in
  `~/.platformio/packages/tool-openocd`.
- [PlatformIO](https://platformio.org/) (CLI or VS Code) and Python 3, to build the firmware.
- From this folder (`ut334a/` in the fork): `fm33-stlink.cfg` and `tools/`. The config is read-only (no flash
  bank); only `tools/fm33_flash.sh` (with `tools/fm33_flash.tcl`) erases and writes, and only with `--erase-it`.
- A bash shell (Git Bash on Windows) for the scripts.
- A charged UT334A battery, a fine soldering iron (or pogo pins), and tweezers or a push-button.

## Safety
- **High voltage:** about 400 V on the HV section (LM3478 converter, 1.5 mH inductor, diode multiplier, tube
  ends). The SWD pads are next to the MCU, away from it, but do not touch the HV parts while the unit is open.
- **Never connect the ST-Link 3.3 V pin.** The board runs from its own battery (VDD is 3.0 V). On the ST-Link
  supply the chip browns out during erase/program and the flash ends up half written.
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

## 2. Why you must hold NRST (stock firmware only)
The stock firmware turns the SWD pins into outputs right after boot, so the debugger can only connect while the
chip is held in reset. The config then stops the CPU at its very first instruction, before the firmware runs, and
reads the flash. The core's ID (CPUID 0x410CC300) is rejected by OpenOCD's normal Cortex-M target, which is why
the config uses plain memory access (`mem_ap`). Once Rad Pro is installed, the SWD pins stay free and no NRST
hold is needed any more (`--no-nrst` below).

## 3. Back up the stock firmware
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

Verify:
```
sha256sum ut334a_stock_*.bin
```
All three must be identical. If they differ, the link was unreliable: dump again. The unit used for this port
had `6815991f2185751a0f31808c5e766a3113ab04f9bfa443a6e09d5304b611d939`; a different hash may just be a different
stock firmware version. Also check that the files are not all `0x00` or all `0xFF` (that means the core was not
halted when reading).

Store the backup in at least two places (for example the PC and a USB stick or private cloud storage).
Do not publish it: it is UNI-T's copyrighted firmware.

## 4. Build Rad Pro
From the fork's root:
```bash
cd platform.io
pio run -e uni-t-ut334a-en            # or another language: uni-t-ut334a-de, -fr, ...
mkdir -p ~/radpro-signed && cd ~/radpro-signed
python <fork>/tools/sign.py           # signs every built image into ./uni-t-ut334a/firmware/
```
In Git Bash on Windows `pio` may not be on the PATH: use `~/.platformio/penv/Scripts/pio`. The image to flash is
`uni-t-ut334a/firmware/radpro-uni-t-ut334a-en-<version>.bin` (128 KB). Always flash the **signed** image.

## 5. Flash
Run the scripts from the `ut334a/` folder (they look for `fm33-stlink.cfg` there). Battery connected, ST-Link
3.3 V unconnected.

1. **Connection test (read-only):** `tools/fm33_flash.sh dryrun`. It must print `UNPROTECTED`.
2. **Flash:** `tools/fm33_flash.sh flash <signed.bin> --erase-it`. It erases all 512 pages of the main flash,
   programs the image, reads back all 256 KB and compares (≈ 6 min at 125 kHz), then resets the unit.
   It must end with `VERIFY OK`.

NRST: while the **stock** firmware is on the chip, hold pad 1 to GND as in step 3 and release it when the script
prints `ARMED`. With Rad Pro (or a blank chip) on it, add `--no-nrst` as the **last** argument instead and leave
pad 1 alone: the script halts the core by a software reset. Example update of an installed Rad Pro:
```bash
tools/fm33_flash.sh dryrun --no-nrst
tools/fm33_flash.sh flash radpro-uni-t-ut334a-en-<version>.bin --erase-it --no-nrst
```

If it reports the flash controller **locked** ("Nothing was erased"): disconnect the battery for 5 s, reconnect,
retry. If it reports `VERIFY FAILED`: do not power-cycle, flash again (or restore, below).

After a good flash, hold OK for about 3 s to power on. Settings are under the menu (OK); see the
[Rad Pro user's manual](../../docs/users-manual.md).

## Restoring the stock firmware
`tools/fm33_flash.sh flash <your dump>.bin --erase-it --no-nrst` (≈ 23 min; Rad Pro leaves SWD free, so no NRST
hold). In the port's private workspace `tools/fm33_flash.sh restore --erase-it` does the same with
`backup/ut334a_stock_1.bin` after checking its SHA-256. The restore path is verified against a strict model of
the flash controller (`tools/fm33_fakefls.tcl`) but has **not yet been run on real hardware**.

## If the unit stays dark after a failed session
Debug settings survive a normal reset. Hold NRST, run
`openocd -s <openocd>/openocd/scripts -f fm33-stlink.cfg -c "init; source tools/fm33_clear_debug.tcl"`,
then release NRST. Or disconnect the battery for 5 s. If the flash was left half written, flash again
(with Rad Pro gone, SWD is free: `--no-nrst` works).
