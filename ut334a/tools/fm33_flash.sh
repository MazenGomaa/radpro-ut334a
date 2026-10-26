#!/bin/bash
# Flash or restore the UT334A (FM33LC043N) over SWD with the ST-Link, holding NRST by hand.
#
#   tools/fm33_flash.sh dryrun                       read-only: connect, halt, check protection, release
#   tools/fm33_flash.sh flash <signed.bin> --erase-it   erase all 512 pages, program the image, verify, reset
#   tools/fm33_flash.sh restore --erase-it           same with backup/ut334a_stock_1.bin (SHA-256 checked first)
#   add --no-nrst (last) to halt by SYSRESETREQ instead: only when the stock firmware is not on the chip
#
# Power the board from its battery only: on the ST-Link 3.3 V pin the chip browns out during erase/program.
#
# Wiring and NRST procedure: docs/swd-access.md. Hold NRST (pad 1) to GND, start the script, release NRST
# when it prints ARMED. Run from the workspace root.
set -e
cd "$(dirname "$0")/.."
O=~/.platformio/packages/tool-openocd
OPENOCD="$O/bin/openocd.exe -s $O/openocd/scripts -f fm33-stlink.cfg -f tools/fm33_flash.tcl"
STOCK_SHA=6815991f2185751a0f31808c5e766a3113ab04f9bfa443a6e09d5304b611d939
work=$(mktemp -d)
HALT='fm33_arm; if {![fm33_wait_halt]} { fm33_release; shutdown };'
if [ "${@: -1}" = --no-nrst ]; then
    HALT='if {![fm33_reset_halt]} { fm33_release; shutdown };'
    set -- "${@:1:$(($# - 1))}"
fi

run_session() {
    # Retry until the debug port answers (only while NRST is held)
    local cmds="$1" end=$((SECONDS + 180)) out
    while [ $SECONDS -lt $end ]; do
        out=$(timeout 1800 $OPENOCD -c "init; $cmds" 2>&1) || true
        if echo "$out" | grep -q DPIDR; then
            echo "$out" | grep -v '^Info : \(clock speed\|STLINK\|Target voltage\|\[fm33lc0\)'
            return 0
        fi
    done
    echo "No SWD connection within 180 s." >&2
    return 1
}

SESSION_HEAD="$HALT"' if {![fm33_protection]} { echo ">>> ABORT: not unprotected"; fm33_release; shutdown };'

case "$1" in
dryrun)
    run_session "$SESSION_HEAD"' echo [format "FLS_EPCR 0x%08X FLS_ISR 0x%08X RCC_PCLKCR2 0x%08X" [fm33_rd $FM33_FLS_EPCR] [fm33_rd $FM33_FLS_ISR] [fm33_rd $FM33_RCC_PCLKCR2]]; set t [clock milliseconds]; for {set i 0} {$i < 100} {incr i} { fm33_rd 0x0 }; echo "100 reads: [expr {[clock milliseconds] - $t}] ms"; fm33_release; shutdown'
    ;;
flash|restore)
    if [ "$1" = flash ]; then image="$2"; confirm="$3"; else image=backup/ut334a_stock_1.bin; confirm="$2"; fi
    [ "$confirm" = --erase-it ] || { echo "Refusing: add --erase-it (erases the whole main flash)." >&2; exit 1; }
    [ -f "$image" ] || { echo "No such image: $image" >&2; exit 1; }
    if [ "$1" = restore ]; then
        echo "$STOCK_SHA  $image" | sha256sum -c - || { echo "Backup checksum mismatch, refusing." >&2; exit 1; }
    fi
    python -I tools/fm33_flashgen.py "$image" "$work/program.tcl" "$work/expected.bin"
    cp "$work/program.tcl" tools/.fm33_program.tcl
    run_session "$SESSION_HEAD"' if {[catch {source tools/.fm33_program.tcl} e]} { echo ">>> ERROR: $e"; fm33_flash_end; fm33_release; shutdown }; dump_image tools/.fm33_readback.bin 0x0 0x40000; echo ">>> Readback written."; fm33_reset_run; shutdown' | tee "$work/session.log"
    rm -f tools/.fm33_program.tcl
    if grep -q 'Nothing was erased' "$work/session.log"; then
        echo ">>> Flash untouched (controller locked). Disconnect the battery for 5 s, then retry." >&2; exit 1
    fi
    if [ -f tools/.fm33_readback.bin ] && cmp -s tools/.fm33_readback.bin "$work/expected.bin"; then
        echo ">>> VERIFY OK: readback matches $image ($(sha256sum tools/.fm33_readback.bin | cut -c1-16)...)"
    else
        echo ">>> VERIFY FAILED (readback missing or different). Do not power-cycle; run restore." >&2
        exit 1
    fi
    rm -f tools/.fm33_readback.bin
    ;;
*)
    sed -n '2,12p' "$0"; exit 1 ;;
esac
