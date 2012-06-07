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

#include "calibrator/XorgPrint.hpp"

#include <cstdio>

CalibratorXorgPrint::CalibratorXorgPrint(const char* const device_name0, const XYinfo& axys0, const int thr_misclick, const int thr_doubleclick, const OutputType output_type, const char* geometry)
  : Calibrator(device_name0, axys0, thr_misclick, thr_doubleclick, output_type, geometry)
{
    printf("Calibrating standard Xorg driver \"%s\"\n", device_name);
    printf("\tcurrent calibration values: min_x=%d, max_x=%d and min_y=%d, max_y=%d\n",
                old_axys.x.min, old_axys.x.max, old_axys.y.min, old_axys.y.max);
    printf("\tIf these values are estimated wrong, either supply it manually with the --precalib option, or run the 'get_precalib.sh' script to automatically get it (through HAL).\n");
}

bool CalibratorXorgPrint::finish_data(const XYinfo new_axys)
{
    bool success = true;


    printf("\t--> Making the calibration permanent <--\n");
    switch (output_type) {
        case OUTYPE_AUTO:
            // xorg.conf.d or alternatively hal config
            if (has_xorgconfd_support()) {
                success &= output_xorgconfd(new_axys);
            } else {
                success &= output_hal(new_axys);
            }
            break;
        case OUTYPE_XORGCONFD:
            success &= output_xorgconfd(new_axys);
            break;
        case OUTYPE_HAL:
            success &= output_hal(new_axys);
            break;
        default:
            fprintf(stderr, "ERROR: XorgPrint Calibrator does not support the supplied --output-type\n");
            success = false;
    }

    return success;
}

bool CalibratorXorgPrint::output_xorgconfd(const XYinfo new_axys)
{
    const char* sysfs_name = get_sysfs_name();
    bool not_sysfs_name = (sysfs_name == NULL);
    if (not_sysfs_name)
        sysfs_name = "!!Name_Of_TouchScreen!!";

    // xorg.conf.d snippet
    printf("  copy the snippet below into '/etc/X11/xorg.conf.d/99-calibration.conf'\n");
    printf("Section \"InputClass\"\n");
    printf("	Identifier	\"calibration\"\n");
    printf("	MatchProduct	\"%s\"\n", sysfs_name);
    printf("	Option	\"MinX\"	\"%d\"\n", new_axys.x.min);
    printf("	Option	\"MaxX\"	\"%d\"\n", new_axys.x.max);
    printf("	Option	\"MinY\"	\"%d\"\n", new_axys.y.min);
    printf("	Option	\"MaxY\"	\"%d\"\n", new_axys.y.max);
    printf("	Option	\"SwapXY\"	\"%d\" # unless it was already set to 1\n", new_axys.swap_xy);
    printf("	Option	\"InvertX\"	\"%d\"  # unless it was already set\n", new_axys.x.invert);
    printf("	Option	\"InvertY\"	\"%d\"  # unless it was already set\n", new_axys.y.invert);
    printf("EndSection\n");

    if (not_sysfs_name)
        printf("\nChange '%s' to your device's name in the config above.\n", sysfs_name);

    return true;
}

bool CalibratorXorgPrint::output_hal(const XYinfo new_axys)
{
    const char* sysfs_name = get_sysfs_name();
    bool not_sysfs_name = (sysfs_name == NULL);
    if (not_sysfs_name)
        sysfs_name = "!!Name_Of_TouchScreen!!";

    // HAL policy output
    printf("  copy the policy below into '/etc/hal/fdi/policy/touchscreen.fdi'\n\
<match key=\"info.product\" contains=\"%s\">\n\
  <merge key=\"input.x11_options.minx\" type=\"string\">%d</merge>\n\
  <merge key=\"input.x11_options.maxx\" type=\"string\">%d</merge>\n\
  <merge key=\"input.x11_options.miny\" type=\"string\">%d</merge>\n\
  <merge key=\"input.x11_options.maxy\" type=\"string\">%d</merge>\n"
     , sysfs_name, new_axys.x.min, new_axys.x.max, new_axys.y.min, new_axys.y.max);
    printf("  <merge key=\"input.x11_options.swapxy\" type=\"string\">%d</merge>\n", new_axys.swap_xy);
    printf("  <merge key=\"input.x11_options.invertx\" type=\"string\">%d</merge>\n", new_axys.x.invert);
    printf("  <merge key=\"input.x11_options.inverty\" type=\"string\">%d</merge>\n", new_axys.y.invert);
    printf("</match>\n");

    if (not_sysfs_name)
        printf("\nChange '%s' to your device's name in the config above.\n", sysfs_name);

    return true;
}
