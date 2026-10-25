# Walk the stock firmware from reset through a list of breakpoint addresses (FPB hardware
# breakpoints, debug registers only) and report the last one reached while SWD still worked.
# Use with fm33-stlink.cfg: hold NRST low, run
#   openocd -f fm33-stlink.cfg -c "init; source tools/swd_bisect.tcl"
# and release NRST when it says ARMED.
#
# WARNING: when the SWD link is lost mid-run, the FPB breakpoint and C_DEBUGEN stay set (they survive
# pin reset), so the firmware halts at the next breakpoint on every boot and the unit stays dark.
# Afterwards run tools/fm33_clear_debug.tcl under a manual NRST hold, or power-cycle the battery.

# Default: main() init calls and the start of its loop. Override with
#   -c "set POINTS {0x... 0x...}" before sourcing.
# Result 2026-10-25: SWD dies inside the call at 0xDC7A (function 0x6A90, peripheral init).
if {![info exists POINTS]} {
    set POINTS {0xDC68 0xDC6E 0xDC72 0xDC76 0xDC7A 0xDC7E 0xDC82 0xDC8E 0xDC92 0xDC96 0xDC9A 0xDCAC}
}

proc read_pc {} {
    write_memory 0xE000EDF4 32 15
    return [fm33_rd32 0xE000EDF8]
}

# FPBv1 (Cortex-M0): COMP = addr[28:2] | REPLACE(01 low half / 10 high half) << 30 | ENABLE
proc fpb_set {n addr} {
    set rep [expr {($addr & 2) ? 2 : 1}]
    write_memory [expr {0xE0002008 + 4 * $n}] 32 [expr {($addr & 0x1FFFFFFC) | ($rep << 30) | 1}]
}
proc fpb_clear {} { foreach n {0 1 2 3} { write_memory [expr {0xE0002008 + 4 * $n}] 32 0 } }

fm33_arm
if {![fm33_wait_halt]} { fm33_release; shutdown; return }
write_memory 0xE000EDFC 32 [expr {[fm33_rd32 0xE000EDFC] & ~1}]
write_memory 0xE0002000 32 3   ;# FP_CTRL: KEY | ENABLE
echo "FP_CTRL: [format 0x%08X [fm33_rd32 0xE0002000]]"

set reached "reset vector"
foreach a $POINTS {
    if {[catch {
        fpb_clear
        fpb_set 0 $a
        write_memory 0xE000EDF0 32 0xA05F0001   ;# run
        set hit 0
        for {set t 0} {$t < 3000} {incr t 10} {
            if {[fm33_rd32 0xE000EDF0] & 0x20000} { set hit 1; break }
            sleep 10
        }
        if {!$hit} { write_memory 0xE000EDF0 32 0xA05F0003 }
        set pc [format 0x%04X [read_pc]]
    } e]} {
        echo ">>> SWD lost while running from $reached towards [format 0x%04X $a]"
        shutdown
        return
    }
    if {$hit} {
        echo "Reached [format 0x%04X $a] (PC $pc), SWD alive"
        set reached [format 0x%04X $a]
    } else {
        echo "Did not reach [format 0x%04X $a] within 3 s (halted at PC $pc), SWD alive"
        set reached "PC $pc"
    }
}
echo ">>> SWD still alive after all points"
catch { fpb_clear; write_memory 0xE0002000 32 2; fm33_release }
shutdown
