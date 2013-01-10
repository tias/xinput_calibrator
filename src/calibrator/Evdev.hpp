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
                    const bool use_timeout=false);

public:
    CalibratorEvdev(const char* const device_name,
                    const XYinfo& axys,
                    XID device_id=(XID)-1,
                    const int thr_misclick=0,
                    const int thr_doubleclick=0,
                    const OutputType output_type=OUTYPE_AUTO,
                    const char* geometry=0,
                    const bool use_timeout=false);
    ~CalibratorEvdev();

    // Called by base class finish() method.
    // Allows for derived class specific "adjustments" to be made
    // just prior to the scaling and calibration calculations being
    // performed.
    //
    // @param width
    // The screen width, in pixels, as passed to calibrator::finish()
    //
    // @param height
    // The screen height, in pixels, as passed to calibrator::finish()
    //
    // @param x_min
    // The mean minimum x coordinate.  Calculated in Calibrator::finish()
    // like so: float x_min = (clicked.x[UL] + clicked.x[LL])/2.0;
    //
    // @param y_min
    // The mean minimum y coordinate.  Calculated in Calibrator::finish()
    // like so: float y_min = (clicked.y[UL] + clicked.y[UR])/2.0;
    //
    // @param x_max
    // The mean maximum x coordinate.  Calculated in Calibrator::finish()
    // like so: float x_max = (clicked.x[UR] + clicked.x[LR])/2.0;
    //
    // @param y_max
    // The mean maximum x coordinate.  Calculated in Calibrator::finish()
    // like so: float y_max = (clicked.y[LL] + clicked.y[LR])/2.0;
    //
    // @param new_axis
    // The axes calibration data, of type XYinfo, being set by calibrator:finish().
    // This is ultimately passed as an arg to finish_data().
    virtual void compensateForDevice( int width, int height, float &x_min, float &y_min,
                                      float &x_max, float &y_max, XYinfo &new_axis );
 
    /// calculate and apply the calibration
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
