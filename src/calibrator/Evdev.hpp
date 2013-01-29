/*
 * Copyright (c) 2009 Tias Guns
 * Copyright 2007 Peter Hutterer (xinput_ methods from xinput)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef CALIBRATOR_EVDEV_HPP
#define CALIBRATOR_EVDEV_HPP

#include "calibrator.hh"
#include <X11/extensions/XInput.h>

/***************************************
 * Class for dynamic evdev calibration
 * uses xinput "Evdev Axis Calibration"
 ***************************************/
class CalibratorEvdev: public Calibrator
{
private:
    Display     *display;
    XDeviceInfo *devInfo;
    XDevice     *dev;

protected:
    // protected constructor: should only be used by subclasses!
    // (pass-through to Calibrator)
    CalibratorEvdev(const char* const device_name,
                    const XYinfo& axys,
                    const int thr_misclick=0,
                    const int thr_doubleclick=0,
                    const OutputType output_type=OUTYPE_AUTO,
                    const char* geometry=0,
                    const bool use_timeout=false,
                    const char* output_filename = 0);

public:
    CalibratorEvdev(const char* const device_name,
                    const XYinfo& axys,
                    XID device_id=(XID)-1,
                    const int thr_misclick=0,
                    const int thr_doubleclick=0,
                    const OutputType output_type=OUTYPE_AUTO,
                    const char* geometry=0,
                    const bool use_timeout=false,
                    const char* output_filename = 0);
    virtual ~CalibratorEvdev();

    /// calculate and apply the calibration
    virtual bool finish(int width, int height);
    virtual bool finish_data(const XYinfo new_axys);

    bool set_swapxy(const int swap_xy);
    bool set_invert_xy(const int invert_x, const int invert_y);
    bool set_calibration(const XYinfo new_axys);

    // xinput_ functions (from the xinput project)
    Atom xinput_parse_atom(Display *display, const char* name);
    XDeviceInfo* xinput_find_device_info(Display *display, const char* name, Bool only_extended);
    bool xinput_do_set_int_prop( const char * name,
                                 Display *display,
                                 int format,
                                 int argc,
                                 const int* argv);
protected:
    bool output_xorgconfd(const XYinfo new_axys);
    bool output_hal(const XYinfo new_axys);
    bool output_xinput(const XYinfo new_axys);
};

#endif
