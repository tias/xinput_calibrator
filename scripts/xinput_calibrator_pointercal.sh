#!/bin/sh
# script to make the changes permanent (xinput is called with every Xorg start)
#
# can be used from Xsession.d
# script needs tee and sed (busybox variants are enough)
#
# original script: Martin Jansa <Martin.Jansa@gmail.com>, 2010-01-31
# updated by Tias Guns <tias@ulyssis.org>, 2010-02-15
# updated by Koen Kooi <koen@dominion.thruhere.net>, 2012-02-28
# updated by Al Nikolov <alexey.nikolov@etteplan.com>, 2020-05-14

PATH="/usr/bin:$PATH"

BINARY="xinput_calibrator"
CALFILE="/etc/X11/xorg.conf.d/99-calibration.conf"
LOGFILE="/var/log/xinput_calibrator.pointercal.log"

if [ -e $CALFILE ] ; then
  if grep replace $CALFILE ; then
    echo "Empty calibration file found"
  else
    echo "Using calibration data stored in $CALFILE"
    exit 0
  fi
fi

$BINARY --output-type xorg.conf.d -v --output-filename $CALFILE | tee $LOGFILE
echo "Calibration data stored in $CALFILE (log in $LOGFILE)"
