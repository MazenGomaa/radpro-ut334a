# Offline test for tools/fm33_flash.tcl: a strict FLS model (keys need the matching EREQ/PREQ, which the
# controller clears after each op; key errors lock it like the chip; after an op, one KEY write of any value
# closes it, while KEY = 0 in the protected state is a key error, as measured on the bench 2026-10-25). Usage, from the workspace root:
#   python -I tools/fm33_flashgen.py <image> /tmp/p.tcl /tmp/expected.bin; tclsh tools/fm33_fakefls.tcl /tmp/p.tcl
# Writes fake_readback.bin next to the script and prints the SWD operation count.
# Strict FM33 FLS model: PREQ/EREQ auto-clear after each op; key without request, or wrong order -> lock (KEYSTA 4, KEYERR)
array set R {0x4000022C 0x80 0x40000270 0 0x40001014 0 0x40001018 0 0x40001020 0}
set mem [dict create]; set keyst 0; set k1 ""; set nops 0; set done 0
proc echo {s} { puts $s }
proc read_memory {a w n} { global R mem keyst nops; incr nops; set a [format 0x%08X $a]
    if {$a eq "0x40001020"} { return [list [expr {$R(0x40001020) | ($keyst << 17)}]] }
    if {[info exists R($a)]} { return [list $R($a)] }
    if {[dict exists $mem $a]} { return [list [dict get $mem $a]] }; return [list 0xFFFFFFFF] }
proc lockup {} { global keyst R; set keyst 4; set R(0x40001020) [expr {$R(0x40001020) | 0x200}] }
proc write_memory {a w v} { global R mem keyst k1 nops done; incr nops; set A [format 0x%08X $a]
    switch $A {
      0x40001020 { set R($A) [expr {$R($A) & ~$v}]; return }
      0x40001014 { set R($A) $v; return }
      0x40001018 { if {$keyst == 4} return
          set e $R(0x40001014)
          if {$done} { set done 0; return }
          if {$k1 eq ""} { if {$v == 0} { if {$keyst == 0} { lockup } else { set keyst 0 }; return }; set k1 $v; return }
          set p [format "%08X/%08X" $k1 $v]; set k1 ""
          if {$p eq "96969696/EAEAEAEA" && ($e & 1) && !($e & 2)} { set keyst 2 } \
          elseif {$p eq "A5A5A5A5/F1F1F1F1" && ($e & 2) && !($e & 1)} { set keyst 3 } else { lockup }
          return }
    }
    if {$a < 0x40000} { if {$keyst == 4} return
        if {$keyst == 2 && $v == 0x1234ABCD} { set b [expr {$a & ~0x1FF}]
            for {set i 0} {$i < 512} {incr i 4} { dict unset mem [format 0x%08X [expr {$b+$i}]] }
            set R(0x40001014) [expr {$R(0x40001014) & ~3}]; set R(0x40001020) [expr {$R(0x40001020)|1}]; set keyst 0; set done 1; return }
        if {$keyst == 3} { set old 0xFFFFFFFF; if {[dict exists $mem $A]} { set old [dict get $mem $A] }
            dict set mem $A [expr {$old & $v}]
            set R(0x40001014) [expr {$R(0x40001014) & ~3}]; set R(0x40001020) [expr {$R(0x40001020)|2}]; set keyst 0; set done 1; return }
        lockup; return }
    set R($A) $v }
source tools/fm33_flash.tcl
if {[catch {source $argv} e]} { puts ">>> ERROR: $e" }
puts [format "final FLS_ISR 0x%08X" [lindex [read_memory 0x40001020 32 1] 0]]
set f [open [file dirname $argv]/fake_readback.bin wb]
for {set a 0} {$a < 0x40000} {incr a 4} { set k [format 0x%08X $a]; set v 0xFFFFFFFF; if {[dict exists $mem $k]} {set v [dict get $mem $k]}; puts -nonewline $f [binary format i $v] }
close $f; puts "SWD ops: $nops"
