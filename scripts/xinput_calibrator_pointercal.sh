#!/bin/sh
# script to make the changes permanent (xinput is called with every Xorg start)
#
# can be used from Xsession.d
# script needs tee and sed (busybox variants are enough)
#
# original script: Martin Jansa <Martin.Jansa@gmail.com>, 2010-01-31
# updated by Tias Guns <tias@ulyssis.org>, 2010-02-15

BINARY="xinput_calibrator"
CALFILE="/etc/pointercal.xinput"
LOGFILE="/var/log/xinput_calibrator.pointercal.log"

if [ -e $CALFILE ] ; then
  echo "Using calibration data stored in $CALFILE"
  . $CALFILE
else
  CALDATA=`$BINARY -v | tee $LOGFILE | grep '    xinput set' | sed 's/^    //g; s/$/;/g'`
  if [ ! -z "$CALDATA" ] ; then
    echo $CALDATA > $CALFILE
    echo "Calibration data stored in $CALFILE (log in $LOGFILE)"
  fi
fi
