# Run the firmware from reset in short bursts (resume, halt, read PC) until the SWD link dies,
# to locate the code that takes SWD away. Use with fm33-stlink.cfg: hold NRST low, run
#   openocd -f fm33-stlink.cfg -c "init; source tools/swd_trace.tcl"
# and release NRST when it says ARMED. Only touches the volatile debug registers.

proc read_pc {} {
    write_memory 0xE000EDF4 32 15
    return [format %08X [fm33_rd32 0xE000EDF8]]
}

fm33_arm
if {![fm33_wait_halt]} { fm33_release; shutdown; return }
write_memory 0xE000EDFC 32 [expr {[fm33_rd32 0xE000EDFC] & ~1}]

set t0 [clock milliseconds]
set trace {}
set lastpc [read_pc]
for {set i 0} {$i < 4000} {incr i} {
    if {[catch {
        write_memory 0xE000EDF0 32 0xA05F0001   ;# resume
        write_memory 0xE000EDF0 32 0xA05F0003   ;# halt
        set pc [read_pc]
    } e]} {
        echo "Burst $i, [expr {[clock milliseconds] - $t0}] ms: SWD lost. Last PC $lastpc"
        break
    }
    if {$pc ne $lastpc} { lappend trace $pc }
    set lastpc $pc
}
echo "Last 60 PCs: [lrange $trace end-59 end]"
catch { fm33_release }
shutdown
