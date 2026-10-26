# SWD access to the UT334A (stock firmware)

## Wiring
| ST-Link | Board pad | MCU (QFN-32) |
|---|---|---|
| GND | pad 5 | GND |
| SWDIO | pad 2 | pin 1, PD8 |
| SWCLK | pad 3 | pin 32, PD7 |
| RST | **leave disconnected** (the clone's RST does not pull NRST low on this board: 139 attempts, 0 connects) | – |
| 3.3 V | **never connect** | – |
| – | pad 1 = NRST (pin 2), pad 4 = VDD 3.0 V | – |

Square pad = pad 1. Hold NRST low by hand (tweezers or a push-button from pad 1 to GND). Never short pad 1 while
the ST-Link RST wire is attached.

## With Rad Pro on the chip (2026-10-26)
- Rad Pro (and a blank chip) leaves PD7/PD8 as SWD: OpenOCD connects **without NRST**, also while the firmware
  runs (register reads/writes live). Halt at the reset vector by vector catch + SYSRESETREQ:
  `tools/fm33_flash.sh dryrun|flash|restore ... --no-nrst`.
- **Battery power only.** On the ST-Link 3.3 V pin the chip browns out under erase/program current (resets
  mid-flash, garbage reads). Rule 6 stands.
- Avoid touching pad 1 during a session: a bounce resets the chip mid-operation.
- The flash controller key lock (see `ut334a-hardware.md` §8) survives SYSRESETREQ; `fm33_flash.sh` refuses to
  start when locked ("nothing was erased") → battery off 5 s, retry.
- 125 kHz SWD (200 kHz not available): ~3.5 ms per operation; Rad Pro flash ≈ 6 min, stock restore ≈ 23 min.

## Why it is awkward
- The stock firmware parks **PD7/PD8 as outputs driven low** early in `main()` (board GPIO init 0x6B64), so SWD
  only answers while NRST is held low, or once the core is halted at the reset vector. Our Rad Pro port must leave
  PD7/PD8 as SWD; then a normal connect works.
- The core reports **CPUID 0x410CC300** (standard Cortex-M0: 0x410CC200), which OpenOCD's `cortex_m` target
  rejects ("Examination failed"). `fm33-stlink.cfg` therefore uses a `mem_ap` target (AP 0): memory reads and
  writes only, no core handling (no `halt`, `reg`, gdb). For Phase 4 flashing/debugging, either a patched target
  or pyOCD with the FM33LC0XX_DFP pack is needed. To be decided.
- While the core is held in reset, flash/RAM/info all read as 0. That is not a protection indicator.
- Debug registers (DHCSR, DEMCR, FPB) are reset only by **power-on reset**, not by NRST.

## Tooling
- OpenOCD: `~/.platformio/packages/tool-openocd/bin/openocd.exe -s ~/.platformio/packages/tool-openocd/openocd/scripts`
  (xPack 0.12.0+dev). Use `transport select swd` (the build rejects `hla_swd`; the HLA path is broken).
- ST-Link clones report firmware V2J45S7, 0483:3748; "target voltage" ~3.22 V is the clone's own rail.
- `fm33-stlink.cfg` (read-only, no flash bank) provides: `fm33_arm`, `fm33_wait_halt`, `fm33_protection`,
  `fm33_dump`, `fm33_release`, `fm33_session`.

## Procedures
Because the connect only works while NRST is low, run OpenOCD in a retry loop (in the background, 2–3 min
window), and tell the user to **hold NRST ~10 s, then release**. Output appears only when the session ends:
```bash
O=~/.platformio/packages/tool-openocd; end=$((SECONDS+180))
while [ $SECONDS -lt $end ]; do
  out=$(timeout 150 $O/bin/openocd.exe -s $O/openocd/scripts -f fm33-stlink.cfg -c "init; fm33_session" 2>&1)
  echo "$out" | grep -q DPIDR && { echo "$out"; break; }
done
```
(`fm33_session` = arm, wait for halt, check protection, dump only if unprotected, release. For a quick test
replace it with e.g. `fm33_arm; fm33_wait_halt; fm33_protection; fm33_release; shutdown`.)
- **Halt at reset vector** (`fm33_arm`, then release NRST): sets DHCSR = 0xA05F0001 (C_DEBUGEN) and
  DEMCR |= 1 (VC_CORERESET); after release the core halts (DHCSR 0x00030003) before the firmware runs.
- **Release** (`fm33_release`): DEMCR &= ~1, DHCSR = 0xA05F0000.
- **Dump** (`fm33_dump`, core halted): flash ×3, info 0x1FFFFA00–0x1FFFFFFF, RAM.
  0x1FFFF000–0x1FFFF9FF bus-faults over SWD; read the info area from 0x1FFFFA00.
- **Experiments**: `tools/swd_watch.tcl`, `tools/swd_trace.tcl`, `tools/swd_bisect.tcl` (FPB breakpoints; set
  `POINTS` with `-c "set POINTS {...}"`).
- **After any experiment that lost the link**, the unit may stay dark (breakpoint/halting debug left set):
  hold NRST and run `tools/fm33_clear_debug.tcl`, or disconnect the battery for 5 s.
- Windows paths inside `-c "source {...}"` must be Windows-style (`cygpath -m`), or relative to the project.

## Protection facts (datasheet §7.4)
- LDT1 option bytes at 0x1FFFFC00: byte 0 = DBRDPEN (0xAA = debug read protection off), byte 1 = ACLKEN
  (0x33 = ACLOCK off), bytes 2–3 = complement. Factory/unprotected = `AA 33 55 CC`.
- This chip: `AA 33 55 CC FF FF 00 00 FF…`, ACLOCK words 0xFFFFFFFF ×2 → **unprotected, no locked blocks**.
- No option byte disables SWD. Leaving DBRDP needs a full chip erase.
- Chip UID (0x1FFFFA10): `060808091B04FF0A0066003E`. VREF_CAL (0x1FFFFB08) = 1645.

## Backup (`backup/`, private)
| File | Content |
|---|---|
| `ut334a_stock_{1,2,3}.bin` | 256 KB flash ×3, SHA-256 `6815991f2185751a0f31808c5e766a3113ab04f9bfa443a6e09d5304b611d939` |
| `ut334a_info_1FFFFA00-1FFFFFFF.bin` | info area incl. option bytes, UID, VREF_CAL (pieces in `backup/info/`) |
| `ut334a_ram_at_reset.bin` | RAM with the core halted at the reset vector |
| `SHA256SUMS` | checksums (verify with `cd backup && sha256sum -c SHA256SUMS`; all OK on 2026-10-25) |

Keep a second copy off this PC. Restore: `tools/fm33_flash.sh restore --erase-it` (checks the SHA-256, erases all
512 pages, programs, compares a full readback). Flashing Rad Pro: `tools/fm33_flash.sh flash <signed.bin> --erase-it`.
Read-only connection test: `tools/fm33_flash.sh dryrun`. Flash used on hardware since 2026-10-25 (with `--no-nrst`
once Rad Pro is on the chip); restore not yet run on hardware. User guide: `docs/install-guide.md`.
