#!/bin/bash

X11DIR=/etc/X11/xorg.conf.d
X11=${X11DIR}/50-touchscreen.conf

DEVICE=`xinput_calibrator --list | egrep "(\".*\")" -o | sed 's/"//g'`

# reset screen
xinput set-int-prop "${DEVICE}" "Evdev Axes Swap" 8 0
xinput set-int-prop "${DEVICE}" "Evdev Axis Calibration" 32 0 32767 0 32767

# run calibrate
xinput_calibrator --device "${DEVICE}"
# check exit code and on error rollback xsettings

mkdir -p ${X11DIR}
calibrate-xinput.sh "${DEVICE}" > ${X11}

