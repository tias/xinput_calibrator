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

#ifndef CALIBRATOR_EVDEV_TESTER_HPP
#define CALIBRATOR_EVDEV_TESTER_HPP

#include "calibrator.hh"
#include "calibrator/Evdev.hpp"

/***************************************
 * Class for testing the evdev
 * calibration routine
 ***************************************/
class CalibratorEvdevTester: public CalibratorTesterInterface, public CalibratorEvdev
{
protected:
    // store the new axis for use in driver emulation
    XYinfo new_axis;

public:
    CalibratorEvdevTester(const char* const device_name, const XYinfo& axys,
        const int thr_misclick=0, const int thr_doubleclick=0,
        const OutputType output_type=OUTYPE_AUTO, const char* geometry=0);

    virtual bool finish_data(const XYinfo new_axis);

    // emulate the driver processing the coordinates in 'raw'
    virtual XYinfo emulate_driver(const XYinfo& raw, bool useNewAxis, const XYinfo& screen, const XYinfo& device);

    virtual void new_axis_print() {
        new_axis.print();
    }

    //* From CalibratorEvdev
    virtual bool add_click(int x, int y) {
        return CalibratorEvdev::add_click(x, y);
    }
    virtual bool finish(int width, int height) {
        return CalibratorEvdev::finish(width, height);
    }
    
    // evdev 2.7.0 EvdevProcessValuators code, modified to fit us
    void evdev_270_processvaluator(const XYinfo& devAxis, const XYinfo& axis, int* vals);
};

#endif
