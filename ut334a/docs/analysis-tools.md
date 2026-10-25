# Stock-firmware analysis toolchain

Everything here only reads `backup/ut334a_stock_1.bin`; nothing talks to the device.

## Installed (outside the project)
- Ghidra 12.2: `C:\Users\mazen\tools\ghidra_12.2_PUBLIC` (SHA-256 matched release notes). Needs **JDK 25**:
  portable Temurin `C:\Users\mazen\tools\jdk-25.0.4.1+1` (checksum matched). System JDK 21 is too old.
- Unicorn 2.1.4 (`pip install --user unicorn`), capstone 5.0.7.
- ARM GCC/objdump: `~/.platformio/packages/toolchain-gccarmnoneeabi/bin/`. `pdftotext`: `/mingw64/bin`.

## Python conventions
- Run tool scripts with `python -P` (keeps the cwd off sys.path but still sees user site-packages; `-I` hides Unicorn).
- `tools/disasm.py` (renamed from `dis.py`, which shadowed the stdlib module).

## Ghidra (headless, PowerShell)
```powershell
$env:JAVA_HOME = "C:\Users\mazen\tools\jdk-25.0.4.1+1"; $env:PATH = "$env:JAVA_HOME\bin;$env:PATH"
$G = "C:\Users\mazen\tools\ghidra_12.2_PUBLIC\support\analyzeHeadless.bat"; $P = "C:\Users\mazen\Desktop\ut334a_radpro"
# full re-import + analysis + export:
& $G "$P\analysis\ghidra_proj" ut334a -import "$P\backup\ut334a_stock_1.bin" -overwrite -loader BinaryLoader -loader-baseAddr 0 `
  -processor "ARM:LE:32:Cortex" -scriptPath "$P\tools\ghidra" -preScript SetupFM33.java "$P\vendor\FM33LC0XX_DFP\SVD\FM33LC0XX.SVD" `
  -postScript ApplyRuntime.java -postScript ExportDecomp.java "$P\analysis\ghidra"
# re-export only (after editing names): -process ut334a_stock_1.bin -noanalysis -postScript ...
```
- `SetupFM33.java`: RAM/peripheral/SCS blocks, SVD register labels, Thumb mode, vector entries, hand-assigned names.
- `ApplyRuntime.java`: names/signatures of the soft-float helpers, plus a few function names.
- `ExportDecomp.java` → `analysis/ghidra/decompiled.c`, `functions.txt`, `strings.txt`.
- `tools/resolve_dat.py image decompiled.c SVD out.c analysis/ram_names.txt` → `decompiled_resolved.c`
  (literal-pool `DAT_` refs replaced by `g_2000XXXX` / register names / floats). Add RAM names to `analysis/ram_names.txt`.
- Caveat: Ghidra's ARM convention puts float args in VFP registers; this firmware is soft-float, so float
  maths in the decompiler output is unreliable → use the emulator.

## Emulator (`tools/emu.py`)
`Firmware(path).call(addr, r0, r1, r2, r3)` runs a function (peripherals = plain RAM, SysTick COUNTFLAG always
set so `delay_ms` returns). Load `analysis/ram_initial.bin` (RAM after the C runtime init, i.e. at `main` entry)
for realistic state. Used to: identify runtime helpers, get defaults (C1/C2), derive the dose formula
(`measurement_tick` 0x5094), decode the register values left by init routines.

## Scripts in `tools/`
| Script | Purpose |
|---|---|
| `periph_report.py image SVD [filter]` | every literal-pool peripheral access, named, with written values |
| `periph_access.py image lo hi` | loads/stores into an address range, following base+offset arithmetic |
| `find_refs.py image lo hi` | literal-pool words in a range and the LDRs that load them |
| `xref.py image functions.txt addr…` | which functions reference an address |
| `callers.py image addr` | BL call sites of a function |
| `call_args.py image start end name=addr…` | calls in a range with constant r0/r1, GPIO set/reset writes |
| `disasm.py image start end` | Thumb disassembly with literal values |
| `svd_map.py` / `svd_fields.py SVD REG [value]` | SVD register map / field decode |

## Firmware landmarks (stock)
Reset 0x17C → SystemInit 0xC6FC → `__main` 0xC0 → `main` 0xDC68. Board init 0x6A90 (GPIO 0x6B64, ADC 0x6950,
backlight ATIM 0x69C4/0x6A1C, COMP2 0x6A4C, tone GPTIM0 0x6C6C/0x6CCC, LPTIM 0x6D18, NVIC 0x6D48, DMA 0x6B34,
PMU 0x6D70). IRQs: COMP 0x2750 (GM pulse), LPTIM 0x6254 (tick), RTC 0x7830, GPIO 0x4F40. LCD: power-up/init
0x594C/0x5960, cmd 0x6208, data8 0x61D4, data16 0x61A0, window 0x5C04, SPI init 0xBA90, DMA send 0xBB04.
Measurement 0x5094, calibration 0x5008, battery 0x5658, keys 0x2484, power-off 0xA884, clock switch 0x6DD8.
Soft-float: f2d 0x1264, d2f 0x6F4, i2f 0x1490, ui2f 0x14A0, ui2d 0x1016, fadd 0xE22C, fmul 0xE360, fdiv 0x12B8,
dmul 0x101C, ddiv 0xAC8, cdcmple 0xE160, cfcmple 0xE2B8, cfrcmple 0xE410.
