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

#include "calibrator/Evdev.hpp"
#include "calibrator/EvdevTester.hpp"

#include <cstdio>

CalibratorEvdevTester::CalibratorEvdevTester(const char* const device_name0, const XYinfo& axys0, const int thr_misclick, const int thr_doubleclick, const OutputType output_type, const char* geometry)
  : CalibratorEvdev(device_name0, axys0, thr_misclick, thr_doubleclick, output_type, geometry)
{
    //printf("Starting test driver\n");
}

bool CalibratorEvdevTester::finish_data(const XYinfo axis)
{
    new_axis = axis;

    return true;
}

XYinfo CalibratorEvdevTester::emulate_driver(const XYinfo& raw, bool useNewAxis, const XYinfo& screen, const XYinfo& device) {
    XYinfo calibAxis;
    if (useNewAxis)
        calibAxis = new_axis;
    else
        calibAxis = old_axys;

    // call evdev's code (minimally modified to fit us)
    int mins[2] = {raw.x.min, raw.y.min};
    evdev_270_processvaluator(device, calibAxis, mins);
    int maxs[2] = {raw.x.max, raw.y.max};
    evdev_270_processvaluator(device, calibAxis, maxs);

    XYinfo result(mins[0], maxs[0], mins[1], maxs[1]);


    // the last step is usually done by the X server,
    // or transparently somewhere on the way
    result.do_xf86ScaleAxis(screen, device);
    return result;
}

// return in 'vals'
void CalibratorEvdevTester::evdev_270_processvaluator(const XYinfo& devAxis, const XYinfo& axis, int* vals)
{
        //int vals[2] = {valX, valY};
        int absinfo_min[2] = {devAxis.x.min, devAxis.y.min};
        int absinfo_max[2] = {devAxis.x.max, devAxis.y.max};

        /*
         * Code from xf86-input-evdev: src/evdev.c
         * function: static void EvdevProcessValuators(InputInfoPtr pInfo)
         * last change: 2011-12-14 'Fix absolute events with swapped axes'
         * last change id: 8d6dfd13b0c4177305555294218e366a6cddc83f
         *
         * All valuator_mask_isset() test can be skipped here:
         * its a requirement to have both X and Y coordinates
         */
        int i;

        // if (pEvdev->swap_axes) {
        if (axis.swap_xy) {
            int swapped_isset[2] = {0, 0};
            int swapped_values[2];

            for(i = 0; i <= 1; i++) {
                //if (valuator_mask_isset(pEvdev->vals, i)) {
                    swapped_isset[1 - i] = 1;

                    /* in all sensible cases, the absinfo is the same for the
                     * X and Y axis. In that case, the below simplifies to:
                     * wapped_values[1 - i] = vals[i]
                     * However, the code below accounts for the oddball
                     * device for which this would not be the case.
                     */
                    swapped_values[1 - i] =
                        //xf86ScaleAxis(valuator_mask_get(pEvdev->vals, i),
                        //              pEvdev->absinfo[1 - i].maximum,
                        //              pEvdev->absinfo[1 - i].minimum,
                        //              pEvdev->absinfo[i].maximum,
                        //              pEvdev->absinfo[i].minimum);
                        xf86ScaleAxis(vals[i],
                                      absinfo_max[1 - i],
                                      absinfo_min[1 - i],
                                      absinfo_max[i],
                                      absinfo_min[i]);
                //}
            }

            for (i = 0; i <= 1; i++) {
                if (swapped_isset[i])
                    //valuator_mask_set(pEvdev->vals, i, swapped_values[i]);
                    vals[i] = swapped_values[i];
                //else
                //    valuator_mask_unset(pEvdev->vals, i);
            }
        }

        for (i = 0; i <= 1; i++) {
            int val;
            int calib_min;
            int calib_max;

            //if (!valuator_mask_isset(pEvdev->vals, i))
            //    continue;

            //val = valuator_mask_get(pEvdev->vals, i);
            val = vals[i];

            if (i == 0) {
                //calib_min = pEvdev->calibration.min_x;
                calib_min = axis.x.min;
                //calib_max = pEvdev->calibration.max_x;
                calib_max = axis.x.max;
            } else {
                //calib_min = pEvdev->calibration.min_y;
                calib_min = axis.y.min;
                //calib_max = pEvdev->calibration.max_y;
                calib_max = axis.y.max;
            }

            //if (pEvdev->flags & EVDEV_CALIBRATED)
            if (true)
                //val = xf86ScaleAxis(val, pEvdev->absinfo[i].maximum,
                //                    pEvdev->absinfo[i].minimum, calib_max,
                //                    calib_min);
                val = xf86ScaleAxis(val, absinfo_max[i],
                                    absinfo_min[i], calib_max,
                                    calib_min);

            //if ((i == 0 && pEvdev->invert_x) || (i == 1 && pEvdev->invert_y))
            if ((i == 0 && axis.x.invert) || (i == 1 && axis.y.invert))
                //val = (pEvdev->absinfo[i].maximum - val +
                //       pEvdev->absinfo[i].minimum);
                val = (absinfo_max[i] - val +
                       absinfo_min[i]);

            //valuator_mask_set(pEvdev->vals, i, val);
            vals[i] = val;
        }

        //return vals;
}
