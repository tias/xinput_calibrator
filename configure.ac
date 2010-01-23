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

AC_PREREQ([2.57])
AC_INIT(xinput_calibrator,[0.5.0], [http://github.com/tias/xinput_calibrator])
AM_INIT_AUTOMAKE([foreign dist-bzip2])
AM_MAINTAINER_MODE

AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL

AC_ARG_ENABLE([gtkmm], AS_HELP_STRING([--enable-gtkmm], [Enable GTKMM GUI]))
AS_IF([test "x$enable_gtkmm" = "xyes"], [
	PKG_CHECK_MODULES(GTKMM, [gtkmm-2.4])
	AC_SUBST(GTKMM_CFLAGS)
	AC_SUBST(GTKMM_LIBS)
])

AM_CONDITIONAL([BUILD_GTKMM], [test "x$enable_gtkmm" = "xyes"])

PKG_CHECK_MODULES(XORG, [x11 xi])
AC_SUBST(XORG_CFLAGS)
AC_SUBST(XORG_LIBS)
AC_SUBST(VERSION)

AC_CONFIG_FILES(Makefile)
AC_OUTPUT
