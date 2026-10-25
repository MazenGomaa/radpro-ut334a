# FM33LC0xx flash erase/program over SWD (OpenOCD, mem_ap target), for the UT334A Rad Pro port.
#
# Load AFTER fm33-stlink.cfg, which stays read-only on its own:
#   openocd -f fm33-stlink.cfg -f tools/fm33_flash.tcl -c "init; ..."
# Use it through tools/fm33_flash.sh, which generates the erase/program script, holds the safety checks and
# verifies the result. Nothing here runs by itself: loading this file only defines procs.
#
# Sequence: the FMSH flash algorithm (FM33LC04X_FLASH256.FLM, Init/EraseSector/ProgramPage) and datasheet
# 7.4.3, which allows page erase and single-word programming from SWD. Page = 512 bytes. No mass erase,
# no option bytes, no LDT/information area: the procs refuse addresses outside the 256 KB main array.
#
# The core must be halted at its reset vector first (fm33_arm while NRST is held, release, fm33_wait_halt),
# so the stock firmware cannot take the SWD pins.

set FM33_RCC_PCLKCR2 0x4000022C
set FM33_RCC_OPCCR2  0x40000270
set FM33_FLS_EPCR    0x40001014
set FM33_FLS_KEY     0x40001018
set FM33_FLS_ISR     0x40001020
set FM33_AIRCR       0xE000ED0C

set FM33_FLASH_END   0x00040000
set FM33_PAGE_SIZE   0x200

# FLS_ISR: ERD bit 0, PRD bit 1, errors CKERR/KEYERR/AUTHERR/OTPERR bits 8..11
set FM33_ISR_ERD     0x1
set FM33_ISR_PRD     0x2
set FM33_ISR_ERRORS  0xF00

proc fm33_wr {a v} { write_memory $a 32 $v }
proc fm33_rd {a} { return [lindex [read_memory $a 32 1] 0] }

proc fm33_check_main_array {addr} {
    global FM33_FLASH_END
    if {$addr < 0 || $addr >= $FM33_FLASH_END || ($addr & 3)} {
        error [format "address 0x%08X outside the main flash array" $addr]
    }
}

# Flash register clock and erase/program clock (FLM Init), clear stale flags
proc fm33_flash_begin {} {
    global FM33_RCC_PCLKCR2 FM33_RCC_OPCCR2 FM33_FLS_ISR FM33_ISR_ERD FM33_ISR_PRD FM33_ISR_ERRORS
    fm33_wr $FM33_RCC_PCLKCR2 [expr {[fm33_rd $FM33_RCC_PCLKCR2] | 0x20}]
    fm33_wr $FM33_RCC_OPCCR2 [expr {[fm33_rd $FM33_RCC_OPCCR2] | 0x400000}]
    fm33_wr $FM33_FLS_ISR [expr {$FM33_ISR_ERD | $FM33_ISR_PRD | $FM33_ISR_ERRORS}]
}

proc fm33_flash_end {} {
    global FM33_RCC_OPCCR2 FM33_FLS_KEY FM33_FLS_EPCR
    fm33_wr $FM33_FLS_KEY 0
    fm33_wr $FM33_FLS_EPCR 0
    fm33_wr $FM33_RCC_OPCCR2 [expr {[fm33_rd $FM33_RCC_OPCCR2] & ~0x400000}]
}

proc fm33_wait_flag {flag what addr} {
    global FM33_FLS_ISR FM33_ISR_ERRORS FM33_FLS_KEY
    for {set i 0} {$i < 200} {incr i} {
        set isr [fm33_rd $FM33_FLS_ISR]
        if {$isr & ($flag | $FM33_ISR_ERRORS)} { break }
    }
    fm33_wr $FM33_FLS_ISR [expr {$flag | $FM33_ISR_ERRORS}]
    fm33_wr $FM33_FLS_KEY 0
    if {($isr & $FM33_ISR_ERRORS) || !($isr & $flag)} {
        error [format "%s failed at 0x%08X (FLS_ISR 0x%08X)" $what $addr $isr]
    }
}

proc fm33_erase_page {addr} {
    global FM33_FLS_EPCR FM33_FLS_KEY FM33_ISR_ERD FM33_PAGE_SIZE
    fm33_check_main_array $addr
    if {$addr & ($FM33_PAGE_SIZE - 1)} { error [format "0x%08X is not page aligned" $addr] }
    # ERTYPE = 00 (page), PREQ = 0, EREQ = 1
    fm33_wr $FM33_FLS_EPCR [expr {([fm33_rd $FM33_FLS_EPCR] & ~0x303) | 0x1}]
    fm33_wr $FM33_FLS_KEY 0x96969696
    fm33_wr $FM33_FLS_KEY 0xEAEAEAEA
    fm33_wr $addr 0x1234ABCD
    fm33_wait_flag $FM33_ISR_ERD "page erase" $addr
}

proc fm33_program_words {addr words} {
    global FM33_FLS_EPCR FM33_FLS_KEY FM33_ISR_PRD
    # EREQ = 0, PREQ = 1
    fm33_wr $FM33_FLS_EPCR [expr {([fm33_rd $FM33_FLS_EPCR] & ~0x303) | 0x2}]
    foreach w $words {
        fm33_check_main_array $addr
        fm33_wr $FM33_FLS_KEY 0xA5A5A5A5
        fm33_wr $FM33_FLS_KEY 0xF1F1F1F1
        fm33_wr $addr $w
        fm33_wait_flag $FM33_ISR_PRD "program" $addr
        set addr [expr {$addr + 4}]
    }
}

# Leave debug state and reset the chip so it boots the new image (the halted PC belongs to the old one)
proc fm33_reset_run {} {
    global FM33_DHCSR FM33_DEMCR FM33_AIRCR
    write_memory $FM33_DEMCR 32 [expr {[fm33_rd $FM33_DEMCR] & ~1}]
    write_memory $FM33_DHCSR 32 0xA05F0000
    catch { write_memory $FM33_AIRCR 32 0x05FA0004 }
    echo ">>> Reset requested (SYSRESETREQ)."
}
