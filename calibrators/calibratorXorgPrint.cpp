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
    CalibratorXorgPrint(const char* const drivername, const XYinfo& axys);

    virtual bool finish_data(const XYinfo new_axys, int swap_xy);
};

CalibratorXorgPrint::CalibratorXorgPrint(const char* const drivername0, const XYinfo& axys0)
  : Calibrator(drivername0, axys0)
{
    printf("Calibrating standard Xorg driver \"%s\"\n", drivername);
    printf("\tcurrent calibration values: min_x=%d, max_x=%d and min_y=%d, max_y=%d\n",
                old_axys.x_min, old_axys.x_max, old_axys.y_min, old_axys.y_max);
    printf("\tIf these values are estimated wrong, either supply it manually with the --precalib option, or run the 'get_precalib.sh' script to automatically get it (through HAL).\n");
}

bool CalibratorXorgPrint::finish_data(const XYinfo new_axys, int swap_xy)
{
    // FDI policy output
    printf("\nNew method for making the calibration permanent: create an FDI policy file like /etc/hal/fdi/policy/touchscreen.fdi with:\n\
<match key=\"info.product\" contains=\"%%Name_Of_TouchScreen%%\">\n\
  <merge key=\"input.x11_options.minx\" type=\"string\">%d</merge>\n\
  <merge key=\"input.x11_options.miny\" type=\"string\">%d</merge>\n\
  <merge key=\"input.x11_options.maxx\" type=\"string\">%d</merge>\n\
  <merge key=\"input.x11_options.maxy\" type=\"string\">%d</merge>\n"
         , new_axys.x_min, new_axys.x_max, new_axys.y_min, new_axys.y_max);
    if (swap_xy != 0)
        printf("  <merge key=\"input.x11_options.swapxy\" type=\"string\">%d</merge>\n", swap_xy);
    printf("</match>\n");

    // Xorg.conf output
    printf("\nOld method, edit /etc/X11/xorg.conf and add in the 'Section \"Device\"' of your touchscreen device:\n");
    printf("\tOption\t\"MinX\"\t\t\"%d\"\t# was \"%d\"\n",
                new_axys.x_min, old_axys.x_min);
    printf("\tOption\t\"MaxX\"\t\t\"%d\"\t# was \"%d\"\n",
                new_axys.x_max, old_axys.x_max);
    printf("\tOption\t\"MinY\"\t\t\"%d\"\t# was \"%d\"\n",
                new_axys.y_min, old_axys.y_min);
    printf("\tOption\t\"MaxY\"\t\t\"%d\"\t# was \"%d\"\n",
                new_axys.y_max, old_axys.y_max);
    if (swap_xy != 0)
        printf("\tOption\t\"SwapXY\"\t\"%d\"\n", swap_xy);

    return true;
}
