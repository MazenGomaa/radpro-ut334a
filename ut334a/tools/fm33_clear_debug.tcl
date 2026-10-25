# Clear everything the debug experiments may have left set (FPB breakpoints, vector catch,
# halting debug), so the stock firmware boots normally after NRST is released.
# Debug registers survive pin reset (only power-on reset clears them), so run this after any
# breakpoint experiment that ended with the SWD link lost. Hold NRST low, run
#   openocd -f fm33-stlink.cfg -c "init; source tools/fm33_clear_debug.tcl"
# then release NRST.
foreach n {0 1 2 3} { write_memory [expr {0xE0002008 + 4 * $n}] 32 0 }
write_memory 0xE0002000 32 2                ;# FP_CTRL: KEY, ENABLE=0
write_memory 0xE000EDFC 32 0                ;# DEMCR: no vector catch
write_memory 0xE000EDF0 32 0xA05F0000       ;# DHCSR: halting debug off
echo "FP_CTRL=[format 0x%08X [fm33_rd32 0xE0002000]] DEMCR=[format 0x%08X [fm33_rd32 0xE000EDFC]] DHCSR=[format 0x%08X [fm33_rd32 0xE000EDF0]]"
echo ">>> Debug state cleared. Release NRST."
shutdown
