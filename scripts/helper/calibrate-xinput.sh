#!/bin/bash

# MASTouch USB Touchscreen" - match to 'lshal'
#
# Section "InputClass"
#         Identifier "Touch class"
#         MatchProduct "MASTouch USB Touchscreen"
#         MatchDevicePath "/dev/input/event*"
#         Option  "Calibration"           "57 2156 405 1257"
#         Option  "SwapAxes"              "1"
# EndSection

DEVICE="$1"

OPTION1=$(xinput list-props "${DEVICE}" | grep Calibration | awk '{sub(/,/,"",$5); sub(/,/,"",$6);sub(/,/,"",$7);sub(/,/,"",$8); print $5, $6, $7, $8}')
OPTION2=$(xinput list-props "${DEVICE}" | grep Swap | awk '{sub(/,/,"",$5);  print $5}')

echo 'Section "InputClass"'
echo '        Identifier "evdev pointer catchall"'
echo '        MatchIsPointer "on"'
echo '        MatchDevicePath "/dev/input/event*"'
echo '        Driver "evdev"'
echo '        Option  "Calibration"           "'${OPTION1}'"'
echo '        Option  "SwapAxes"              "'${OPTION2}'"'
echo 'EndSection'

