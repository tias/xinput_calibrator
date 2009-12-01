#!/bin/sh
# Get running calibration data from lshal
# (needed to recalibrate it, most easily fetchable from lshal)

export PATH=".:$PATH"
#FIND="hal-find-by-property"
CAPAB="hal-find-by-capability"
GET="hal-get-property"
BINARY="xinput_calibrator"

if [ "$(which $CAPAB)" = "" ]; then
    echo "Error: Can not find executable $CAPAB"
    exit 1
fi
if [ "$(which $GET)" = "" ]; then
    echo "Error: Can not find executable $GET"
    exit 1
fi


#udis=$($FIND --key input.x11_driver --string evtouch)
udis=$($CAPAB --capability input)

if [ "$udis" = "" ]; then
    echo "HAL Error: No input devices found (tested: info.capabilities 'input'))"
    exit 1
fi


echo "Trying all available input devices:"
# possibly multiple screens, iterate over each possibility
count=0
cmd=""
for udi in $udis; do
    name=$($GET --udi $udi --key info.product)
    minx=$($GET --udi $udi --key input.x11_options.minx 2> /dev/null)
    maxx=$($GET --udi $udi --key input.x11_options.maxx 2> /dev/null)
    miny=$($GET --udi $udi --key input.x11_options.miny 2> /dev/null)
    maxy=$($GET --udi $udi --key input.x11_options.maxy 2> /dev/null)

    # missing values ?
    if [ "$minx" = "" ] || [ "$maxx" = "" ] ||
        [ "$miny" = "" ] || [ "$maxy" = "" ]; then
        if [ "$minx" = "" ] && [ "$maxx" = "" ] &&
            [ "$miny" = "" ] && [ "$maxy" = "" ]; then
            # no calibration data available
            echo "\t'$name': no calibration data available"
        else
            # partial calibration data available ???
            echo "Error: '$name', only partial calibration data available (MinX='$minx' MaxX='$maxx' MinY='$miny' MaxY='$maxy'). All 4 current calibration values are need to recalibrate the device !"
        fi
    else
        count=$((count += 1))
        cmd="$BINARY --precalib $minx $maxx $miny $maxy"
        echo "\t'$name': values found, calibrate by running:"
        echo "$cmd"
    fi
done

if [ $count -gt 1 ]; then
    echo "Found multiple calibratable touchscreen devices, select one from above and execute the calibration program with the given parameters."
else
    if [ $count -eq 0 ]; then
        echo "Warning: No existing calibration data found, no parameters used."
        cmd="$BINARY"
    fi

    if [ "$(which $BINARY)" = "" ]; then
        echo "Error: can not find calibration program ($BINARY), please run it with the above parameters yourself."
    else
        echo "\nRunning calibration program..."
        $cmd
    fi
fi
