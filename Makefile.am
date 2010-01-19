#
# Copyright (c) 2010 Petr Stetiar <ynezz@true.cz>
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

AM_CXXFLAGS = -Wall -ansi -pedantic -W -Wmissing-declarations -Werror

if BUILD_GTKMM
bin_PROGRAMS = xinput_calibrator_x11 xinput_calibrator_gtkmm
xinput_calibrator_gtkmm_SOURCES = main_gtkmm.cpp
xinput_calibrator_gtkmm_CXXFLAGS = $(GTKMM_CFLAGS) $(AM_CXXFLAGS)
xinput_calibrator_gtkmm_LDADD = $(GTKMM_LIBS)
else
bin_PROGRAMS = xinput_calibrator_x11
endif

xinput_calibrator_x11_LDADD = $(XORG_LIBS)
xinput_calibrator_x11_CXXFLAGS = $(XORG_CFLAGS) $(AM_CXXFLAGS)
xinput_calibrator_x11_SOURCES = main_x11.cpp

EXTRA_DIST = autogen.sh
