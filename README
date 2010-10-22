xinput calibrator: A generic touchscreen calibration program for X.Org

Version: 0.7.5
Website: http://www.freedesktop.org/wiki/Software/xinput_calibrator
Source:  http://github.com/tias/xinput_calibrator
Bugs:    http://github.com/tias/xinput_calibrator/issues


Build instructions:
-------------------
./autogen.sh
    Sets up build environment, run ./autogen.sh --help to see the build options
    Notable build options:
    --with-gui=gtkmm        Use gtkmm GUI
    --with-gui=x11          Use native x11 GUI
make
    Builds the software with the configured GUI

Usage:
------
Simply run:
    xinput_calibrator

For more information, run with --help or check the manpage.
The scripts/ directory constains scripts to get calibration from hal or use a pointercal file to reapply xinput commands across reboots


More about the project:
-----------------------
Because all existing calibrators were driver dependent and hard to use, xinput_calibrator was created. The goal of xinput_calibrator is to: 
* work for any Xorg driver (use Xinput to get axis valuators), 
* output the calibration as Xorg.conf, HAL policy and udev rule, 
* support advanced driver options, such as Evdev's dynamic calibration, 
* have a very intuitive GUI (normal X client). 

Xinput_calibrator is based on a simple calibrator that was proposed on the Xorg mailinglist. The first release(v0.2.0) improved upon it by reading axis valuators from Xinput, hence making it generic for all touchscreen drivers. The announcement was done on the Xorg mailinglist, and the code is on Tias' webpage. 

Starting from v0.4.0, it writes Xorg.conf and (HAL) FDI policy file values, and contains a wrapper script to get axis valuator information for the evtouch driver (evtouch does not export the current calibration through its axis valuators). It is also the first program to support dynamic evdev calibration, by using its advanced Xinput functionality.

The v0.5.0 version is written entirely in the X window system, needing no external dependencies. Because of its modular structure, other frontends can be easily created too.

Version v0.6.0 has a proper build system and gained a lot of features thanks to the feedback of different users.

Version 0.7.0 has mis-click detection and proper packaging support: proper make dist, one binary, has manpage, menu entry and icon. DEB and RPM package meta-data in their respective VCS branches.
