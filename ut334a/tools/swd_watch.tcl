# Watch PD7/PD8 (SWD pins) and the CPU after reset, to find out when and why SWD stops answering.
# Use with fm33-stlink.cfg: hold NRST low, run
#   openocd -f fm33-stlink.cfg -c "init; source tools/swd_watch.tcl"
# and release NRST when it says ARMED. Only reads, plus the volatile debug registers DHCSR/DEMCR/DCRSR.

set GPIOD 0x40000CC0
set REGS {INEN 0x00 PUEN 0x04 ODEN 0x08 FCR 0x0C DO 0x10 DIN 0x1C DFS 0x20 ANEN 0x28}

proc gpiod_snapshot {} {
    global GPIOD REGS
    set out {}
    foreach {n o} $REGS { lappend out "$n=[format %04X [fm33_rd32 [expr {$GPIOD + $o}]]]" }
    return [join $out { }]
}

# Halt, read PC via DCRSR/DCRDR, resume. Returns PC as hex string.
proc sample_pc {} {
    write_memory 0xE000EDF0 32 0xA05F0003
    write_memory 0xE000EDF4 32 15
    set pc [format %08X [fm33_rd32 0xE000EDF8]]
    write_memory 0xE000EDF0 32 0xA05F0001
    return $pc
}

fm33_arm
if {![fm33_wait_halt]} { fm33_release; shutdown; return }
echo "At reset vector: [gpiod_snapshot]"

# Clear vector catch, keep C_DEBUGEN, run.
write_memory 0xE000EDFC 32 [expr {[fm33_rd32 0xE000EDFC] & ~1}]
write_memory 0xE000EDF0 32 0xA05F0001
set t0 [clock milliseconds]
echo ">>> Firmware running, watching for 30 s"

set last ""
set lastpc ""
set fails 0
while {[clock milliseconds] - $t0 < 30000} {
    if {[catch {
        set s [gpiod_snapshot]
        set pc [sample_pc]
    } e]} {
        incr fails
        if {$fails == 1} { echo "[expr {[clock milliseconds] - $t0}] ms: SWD read failed (last PC $lastpc): $e" }
        if {$fails > 20} { echo ">>> SWD link lost for good."; break }
        sleep 20
        continue
    }
    if {$fails} { echo "[expr {[clock milliseconds] - $t0}] ms: SWD back after $fails failures"; set fails 0 }
    if {$s ne $last} { echo "[expr {[clock milliseconds] - $t0}] ms: PC=$pc  $s"; set last $s }
    set lastpc $pc
    sleep 50
}
echo "End. Last PC $lastpc"
catch { fm33_release }
shutdown
