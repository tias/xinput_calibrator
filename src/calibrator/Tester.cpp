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

#include "calibrator/Tester.hpp"

#include <cstdio>

CalibratorTester::CalibratorTester(const char* const device_name0, const XYinfo& axys0, const int thr_misclick, const int thr_doubleclick, const OutputType output_type, const char* geometry)
  : Calibrator(device_name0, axys0, thr_misclick, thr_doubleclick, output_type, geometry)
{
    //printf("Starting test driver\n");
}

bool CalibratorTester::finish_data(const XYinfo axis)
{
    new_axis = axis;

    return true;
}

XYinfo CalibratorTester::emulate_driver(const XYinfo& raw, bool useNewAxis, const XYinfo& screen, const XYinfo& device) {
    XYinfo calibAxis;
    if (useNewAxis)
        calibAxis = new_axis;
    else
        calibAxis = old_axys;

    /**
     * The most simple and intuitive calibration implementation
     * if only all drivers sticked to this...
     * Note that axis inversion is automatically supported
     * by the ScaleAxis implementation (swapping max/min on
     * one axis will result in the inversion being calculated)
     */

    // placeholder for the new coordinates
    XYinfo result(raw);

    // swap coordinates if asked
    if (calibAxis.swap_xy) {
        result.x.min = raw.y.min;
        result.x.max = raw.y.max;
        result.y.min = raw.x.min;
        result.y.max = raw.x.max;
    }

    result.do_xf86ScaleAxis(device, calibAxis);

    // the last step is usually done by the X server,
    // or transparently somewhere on the way
    result.do_xf86ScaleAxis(screen, device);
    return result;
}
