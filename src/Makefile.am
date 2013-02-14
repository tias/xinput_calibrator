#
# Copyright (c) 2010 Petr Stetiar <ynezz@true.cz>
# Copyright (c) 2010 Tias Guns <tias@ulyssis.org>
#
# Permission is hereby granted, free of charge, to any person
# obtaining a copy of this software and associated documentation
# files (the "Software"), to deal in the Software without
# restriction, including without limitation the rights to use,
# copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following
# conditions:
# 
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
# OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
# HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
# WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.
#

SUBDIRS = \
	calibrator \
	gui

AM_CXXFLAGS = -Wall -ansi -pedantic

bin_PROGRAMS = xinput_calibrator tester

COMMON_SRCS=calibrator.cpp calibrator/XorgPrint.cpp calibrator/Evdev.cpp calibrator/Usbtouchscreen.cpp main_common.cpp gui/gui_common.cpp

# only one of the BUILD_ flags should be set
if BUILD_X11
xinput_calibrator_SOURCES = gui/x11.cpp main_x11.cpp $(COMMON_SRCS)
xinput_calibrator_LDADD = $(XINPUT_LIBS) $(XRANDR_LIBS) $(X11_LIBS)
xinput_calibrator_CXXFLAGS = $(XINPUT_CFLAGS) $(X11_CFLAGS) $(XRANDR_CFLAGS) $(AM_CXXFLAGS)
endif

if BUILD_GTKMM
xinput_calibrator_SOURCES = gui/gtkmm.cpp main_gtkmm.cpp $(COMMON_SRCS)
xinput_calibrator_LDADD = $(XINPUT_LIBS) $(GTKMM_LIBS)
xinput_calibrator_CXXFLAGS = $(XINPUT_CFLAGS) $(GTKMM_CFLAGS) $(AM_CXXFLAGS)

# only include the needed gtkmm stuff
# lets hope this has no side-effects
xinput_calibrator_LDFLAGS = -Wl,--as-needed
endif

tester_SOURCES = tester.cpp calibrator.cpp calibrator/Tester.cpp calibrator/Evdev.cpp calibrator/EvdevTester.cpp
tester_LDADD = $(XINPUT_LIBS) $(XRANDR_LIBS) $(X11_LIBS)
tester_CXXFLAGS = $(XINPUT_CFLAGS) $(X11_CFLAGS) $(XRANDR_CFLAGS) $(AM_CXXFLAGS)

EXTRA_DIST = \
	calibrator.cpp \
	calibrator.hh \
	main_common.cpp
