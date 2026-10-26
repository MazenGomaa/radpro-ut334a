#!/bin/bash
# Bring-up helper: set the UT334A backlight PWM compare (ATIM_CCR1) live over SWD, with the firmware running.
# Register write only (no flash, no halt); Rad Pro overwrites it on the next brightness change or power cycle.
#
#   tools/fm33_backlight.sh <0..10000>     period is 10000 (ARR 9999); Rad Pro "medium-high" is 5310
#   tools/fm33_backlight.sh                read the current value
set -e
cd "$(dirname "$0")/.."
O=~/.platformio/packages/tool-openocd
if [ -n "$1" ]; then
    [[ "$1" =~ ^[0-9]+$ ]] && [ "$1" -le 10000 ] || { echo "value must be 0..10000" >&2; exit 1; }
    cmd="write_memory 0x4001B034 32 $1;"
fi
"$O/bin/openocd.exe" -s "$O/openocd/scripts" -f fm33-stlink.cfg \
    -c "init; $cmd echo \"CCR1 = [lindex [read_memory 0x4001B034 32 1] 0]\"; shutdown" 2>&1 | grep -E "CCR1|Error"
