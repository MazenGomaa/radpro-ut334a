# UNI-T UT334A port: hardware notes and tools

Everything learned about the UNI-T UT334A (FM33LC043N) while porting Rad Pro to it:

| File | Contents |
|---|---|
| `docs/ut334a-hardware.md` | Hardware map: every pin, display init, HV control, GM front end, calibration, battery, clocks |
| `docs/phase4-plan.md` | Port plan and the porting implications of the findings |
| `docs/swd-access.md` | Connecting an ST-Link (manual NRST hold), protection facts, making a backup |
| `docs/analysis-tools.md` | Ghidra / emulator toolchain used on the stock firmware |
| `docs/ut334a-notes.md` | Chronological log |
| `tools/`, `fm33-stlink.cfg` | OpenOCD config and analysis scripts |

The stock firmware is UNI-T's copyrighted code and is **not** included. Paths like `backup/` in these docs refer to
a private workspace. **Before flashing Rad Pro, dump your own unit's stock firmware** (see `docs/swd-access.md`)
so you can restore it if anything goes wrong.
