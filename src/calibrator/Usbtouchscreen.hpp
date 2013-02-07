/*
 * Copyright (c) 2009 Soren Hauberg
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

#ifndef CALIBRATOR_USBTOUCHSCREEN_HPP
#define CALIBRATOR_USBTOUCHSCREEN_HPP

#include "calibrator.hh"

/**********************************
 * Class for usbtouchscreen driver,
 * writes output parameters to running kernel and to modprobe.conf
 **********************************/
class CalibratorUsbtouchscreen: public Calibrator
{
public:
    CalibratorUsbtouchscreen(const char* const device_name, const XYinfo& axys,
         const int thr_misclick=0, const int thr_doubleclick=0,
        const OutputType output_type=OUTYPE_AUTO, const char* geometry=0,
        const bool use_timeout=false, const char* output_filename = 0);
    virtual ~CalibratorUsbtouchscreen();

    virtual bool finish_data(const XYinfo new_axys);

protected:
    // Globals for kernel parameters from startup.
    // We revert to these if the program aborts
    bool val_transform_xy, val_flip_x, val_flip_y, val_swap_xy;

    // Helper functions
    char yesno(const bool value)
    {
        if (value)
            return 'Y';
        else
            return 'N';
    }

    void read_int_parameter(const char *param, int &value);

    void read_bool_parameter(const char *param, bool &value);

    void write_int_parameter(const char *param, const int value);

    void write_bool_parameter(const char *param, const bool value);
};

#endif
