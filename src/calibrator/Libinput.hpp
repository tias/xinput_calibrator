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

#ifndef CALIBRATOR_LIBINPUT_HPP
#define CALIBRATOR_LIBINPUT_HPP

#include "calibrator.hh"
#include <X11/extensions/XInput.h>
#include <cassert>

struct Mat9 {
    float coeff[9];
    float & operator[](int idx) {
        assert(idx >= 0 && idx < 9);
        return coeff[idx];
    }
    float operator[](int idx) const {
        assert(idx >= 0 && idx < 9);
        return coeff[idx];
    }
    void set (float x0, float x1, float x2, float x3, float x4, float x5,
        float x6, float x7, float x8) {
            coeff[0] = x0; coeff[1] = x1; coeff[2] = x2; coeff[3] = x3;
            coeff[4] = x4; coeff[5] = x5; coeff[6] = x6; coeff[7] = x7;
            coeff[8] = x8;
    }
    Mat9(float x0, float x1, float x2, float x3, float x4, float x5, float x6,
        float x7, float x8) {
            set(x0, x1, x2, x3, x4, x5, x6, x7, x8);
    }
    Mat9() {}
};

/***************************************
 * Class for dynamic evdev calibration
 * uses xinput "Evdev Axis Calibration"
 ***************************************/
class CalibratorLibinput: public Calibrator
{
private:
    Display     *display;
    XDeviceInfo *devInfo;
    XDevice     *dev;
    Mat9        old_coeff;
    bool        reset_data;

protected:
    // protected constructor: should only be used by subclasses!
    // (pass-through to Calibrator)
    CalibratorLibinput( const char* const device_name,
                        const XYinfo& axys,
                        const int thr_misclick=0,
                        const int thr_doubleclick=0,
                        const OutputType output_type=OUTYPE_AUTO,
                        const char* geometry=0,
                        const bool use_timeout=false,
                        const char* output_filename = 0);

public:
    CalibratorLibinput( const char* const device_name,
                        const XYinfo& axys,
                        XID device_id=(XID)-1,
                        const int thr_misclick=0,
                        const int thr_doubleclick=0,
                        const OutputType output_type=OUTYPE_AUTO,
                        const char* geometry=0,
                        const bool use_timeout=false,
                        const char* output_filename = 0);
    virtual ~CalibratorLibinput();

    /// calculate and apply the calibration
    virtual bool finish(int width, int height);
    virtual bool finish_data(const XYinfo &new_axys);
    bool finish_data(const Mat9 &coeff);

    bool set_calibration(const Mat9 &coeff);

    // xinput_ functions (from the xinput project)
    Atom xinput_parse_atom(const char* name);
    XDeviceInfo* xinput_find_device_info(Display *display, const char* name, Bool only_extended);
    bool xinput_do_set_int_prop( const char * name,
                                 Display *display,
                                 int format,
                                 int argc,
                                 const int* argv);

protected:
    bool output_xorgconfd(const Mat9 &coeff);
    bool output_hal(const Mat9 &coeff);
    bool output_xinput(const Mat9 &coeff);

private:
    void setMatrix(const char *name, const Mat9 &coeff);
    void getMatrix(const char *name, Mat9 &coeff);
    int set_prop(Display *display, XDevice *dev, const char *name,
                        const float values[9]);
};

#endif
