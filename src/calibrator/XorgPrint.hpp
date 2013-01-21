/*
 * Copyright (c) 2009 Tias Guns
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

#ifndef CALIBRATOR_XORGPRINT_HPP
#define CALIBRATOR_XORGPRINT_HPP

#include "calibrator.hh"

/***************************************
 * Class for generic Xorg driver,
 * outputs new Xorg.conf and FDI policy, on stdout
 ***************************************/
class CalibratorXorgPrint: public Calibrator
{
public:
    CalibratorXorgPrint(const char* const device_name, const XYinfo& axys,
        const int thr_misclick=0, const int thr_doubleclick=0,
        const OutputType output_type=OUTYPE_AUTO, const char* geometry=0,
        const bool use_timeout=false, const char* output_filename = 0);

    virtual bool finish_data(const XYinfo new_axys);

protected:
    bool output_xorgconfd(const XYinfo new_axys);
    bool output_hal(const XYinfo new_axys);
};

#endif
