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

/***************************************
 * Class for generic Xorg driver,
 * outputs new Xorg.conf and FDI policy, on stdout
 ***************************************/
class CalibratorXorgPrint: public Calibrator
{
public:
    CalibratorXorgPrint(const char* const device_name, const XYinfo& axys, const bool verbose);

    virtual bool finish_data(const XYinfo new_axys, int swap_xy);
};

CalibratorXorgPrint::CalibratorXorgPrint(const char* const device_name0, const XYinfo& axys0, const bool verbose0)
  : Calibrator(device_name0, axys0, verbose0)
{
    printf("Calibrating standard Xorg driver \"%s\"\n", device_name);
    printf("\tcurrent calibration values: min_x=%d, max_x=%d and min_y=%d, max_y=%d\n",
                old_axys.x_min, old_axys.x_max, old_axys.y_min, old_axys.y_max);
    printf("\tIf these values are estimated wrong, either supply it manually with the --precalib option, or run the 'get_precalib.sh' script to automatically get it (through HAL).\n");
}

bool CalibratorXorgPrint::finish_data(const XYinfo new_axys, int swap_xy)
{
    // TODO: detect which are applicable at runtime/in the makefile ?
    printf("\n\n== Applying the calibration ==\n");
    printf("There are multiple ways to do this: the tranditional way (xorg.conf), the new way (udev rule) and the soon deprecated way (HAL policy):\n");

    // Xorg.conf output
    printf("\nxorg.conf: edit /etc/X11/xorg.conf and add in the 'Section \"InputDevice\"' of your device:\n");
    printf("\tOption\t\"MinX\"\t\t\"%d\"\n",
                new_axys.x_min);
    printf("\tOption\t\"MaxX\"\t\t\"%d\"\n",
                new_axys.x_max);
    printf("\tOption\t\"MinY\"\t\t\"%d\"\n",
                new_axys.y_min);
    printf("\tOption\t\"MaxY\"\t\t\"%d\"\n",
                new_axys.y_max);
    if (swap_xy != 0)
        printf("\tOption\t\"SwapXY\"\t\"%d\" # unless it was already set to 1\n", swap_xy);

    // udev rule
    printf("\nudev rule: create the file '/etc/udev/rules.d/99_touchscreen.rules' with: (replace %%Name_Of_TouchScreen%% appropriately)\n\
\tACTION!=\"add|change\", GOTO=\"xorg_touchscreen_end\"\n\
\tKERNEL!=\"event*\", GOTO=\"xorg_touchscreen_end\"\n\
\tATTRS{product}!=\"%%Name_Of_TouchScreen%%\", GOTO=\"xorg_touchscreen_end\"\n\
\tENV{x11_options.minx}=\"%d\"\n\
\tENV{x11_options.maxx}=\"%d\"\n\
\tENV{x11_options.miny}=\"%d\"\n\
\tENV{x11_options.maxy}=\"%d\"\n"
         , new_axys.x_min, new_axys.x_max, new_axys.y_min, new_axys.y_max);
    if (swap_xy != 0)
        printf("\tENV{x11_options.swapxy}=\"%d\"\n", swap_xy);
    printf("\tLABEL=\"xorg_touchscreen_end\"\n");

    // HAL policy output
    printf("\nHAL policy: create the file '/etc/hal/fdi/policy/touchscreen.fdi' with: (replace %%Name_Of_TouchScreen%% appropriately)\n\
\t<match key=\"info.product\" contains=\"%%Name_Of_TouchScreen%%\">\n\
\t  <merge key=\"input.x11_options.minx\" type=\"string\">%d</merge>\n\
\t  <merge key=\"input.x11_options.maxx\" type=\"string\">%d</merge>\n\
\t  <merge key=\"input.x11_options.miny\" type=\"string\">%d</merge>\n\
\t  <merge key=\"input.x11_options.maxy\" type=\"string\">%d</merge>\n"
         , new_axys.x_min, new_axys.x_max, new_axys.y_min, new_axys.y_max);
    if (swap_xy != 0)
        printf("\t  <merge key=\"input.x11_options.swapxy\" type=\"string\">%d</merge>\n", swap_xy);
    printf("\t</match>\n");

    return true;
}
