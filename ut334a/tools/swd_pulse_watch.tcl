# Read-only live pulse counter watch for Rad Pro fork 5531898 (addresses from its ELF; recheck after a reflash).
#   openocd -f fm33-stlink.cfg -c "init; source tools/swd_pulse_watch.tcl"
# Read-only: wait for USB unplug (PA9=0), then plug (PA9=1), then sample the pulse counter for 60 s.
proc rd {a} { return [fm33_rd32 $a] }
proc pa9 {} { return [expr {([rd 0x40000C1C] >> 9) & 1}] }
proc line {t0 lc lt} {
    set tick [rd 0x200005DC]; set cnt [rd 0x200005E0]
    set rate [expr {$tick != $lt ? ($cnt - $lc) * 1000.0 / ($tick - $lt) : 0}]
    echo [format "t=%6d ms count=%d rate=%.1f cps deadMin=%d us PA9=%d" [expr {[clock milliseconds]-$t0}] $cnt $rate [expr {[rd 0x200005FC] & 0xFFFF}] [pa9]]
    return [list $cnt $tick]
}
set w0 [clock milliseconds]
echo "WAIT unplug"
while {[pa9] == 1} { if {[clock milliseconds]-$w0 > 300000} { echo "timeout"; shutdown }; after 50 }
echo "UNPLUGGED count=[rd 0x200005E0]"
while {[pa9] == 0} { if {[clock milliseconds]-$w0 > 300000} { echo "timeout"; shutdown }; after 20 }
set t0 [clock milliseconds]
echo "PLUGGED count=[rd 0x200005E0]"
set lc [rd 0x200005E0]; set lt [rd 0x200005DC]
while {[clock milliseconds] - $t0 < 60000} {
    if {[catch { lassign [line $t0 $lc $lt] lc lt } err]} { echo "read failed: $err" }
    after 250
}
shutdown
