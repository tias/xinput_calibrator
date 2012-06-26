/*
 * Copyright (c) 2009 Tias Guns
 * Copyright (c) 2009 Soren Hauberg
 * Copyright (c) 2011 Antoine Hue
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
#include <algorithm>
#include <sys/types.h>
#include <dirent.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <cmath>

#include "calibrator.hh"

// static instance
bool Calibrator::verbose = false;

Calibrator::Calibrator(const char* const device_name0, const XYinfo& axys0,
    const int thr_misclick, const int thr_doubleclick, const OutputType output_type0, const char* geometry0)
: device_name(device_name0),
    threshold_doubleclick(thr_doubleclick), threshold_misclick(thr_misclick),
    output_type(output_type0), geometry(geometry0)
{
    old_axys = axys0;

    clicked.num = 0;
    //clicked.x(NUM_POINTS);
    //clicked.y(NUM_POINTS);
}

bool Calibrator::add_click(int x, int y)
{
    // Double-click detection
    if (threshold_doubleclick > 0 && clicked.num > 0) {
        int i = clicked.num - 1;
        while (i >= 0) {
            if (abs(x - clicked.x[i]) <= threshold_doubleclick
                && abs(y - clicked.y[i]) <= threshold_doubleclick) {
                if (verbose) {
                    printf("DEBUG: Not adding click %i (X=%i, Y=%i): within %i pixels of previous click\n",
                         clicked.num, x, y, threshold_doubleclick);
                }
                return false;
            }
            i--;
        }
    }

    // Mis-click detection
    if (threshold_misclick > 0 && clicked.num > 0) {
        bool misclick = true;

        switch (clicked.num) {
            case 1:
                // check that along one axis of first point
                if (along_axis(x,clicked.x[UL],clicked.y[UL]) ||
                        along_axis(y,clicked.x[UL],clicked.y[UL]))
                {
                    misclick = false;
                } else if (verbose) {
                    printf("DEBUG: Mis-click detected, click %i (X=%i, Y=%i) not aligned with click 0 (X=%i, Y=%i) (threshold=%i)\n",
                            clicked.num, x, y, clicked.x[UL], clicked.y[UL], threshold_misclick);
                }
                break;

            case 2:
                // check that along other axis of first point than second point
                if ((along_axis( y, clicked.x[UL], clicked.y[UL])
                            && along_axis( clicked.x[UR], clicked.x[UL], clicked.y[UL]))
                        || (along_axis( x, clicked.x[UL], clicked.y[UL])
                            && along_axis( clicked.y[UR], clicked.x[UL], clicked.y[UL])))
                {
                    misclick = false;
                } else if (verbose) {
                    printf("DEBUG: Mis-click detected, click %i (X=%i, Y=%i) not aligned with click 0 (X=%i, Y=%i) or click 1 (X=%i, Y=%i) (threshold=%i)\n",
                            clicked.num, x, y, clicked.x[UL], clicked.y[UL], clicked.x[UR], clicked.y[UR], threshold_misclick);
                }
                break;

            case 3:
                // check that along both axis of second and third point
                if ( ( along_axis( x, clicked.x[UR], clicked.y[UR])
                            &&   along_axis( y, clicked.x[LL], clicked.y[LL]) )
                        ||( along_axis( y, clicked.x[UR], clicked.y[UR])
                            &&  along_axis( x, clicked.x[LL], clicked.y[LL]) ) )
                {
                    misclick = false;
                } else if (verbose) {
                    printf("DEBUG: Mis-click detected, click %i (X=%i, Y=%i) not aligned with click 1 (X=%i, Y=%i) or click 2 (X=%i, Y=%i) (threshold=%i)\n",
                            clicked.num, x, y, clicked.x[UR], clicked.y[UR], clicked.x[LL], clicked.y[LL], threshold_misclick);
                }
        }

        if (misclick) {
            reset();
            return false;
        }
    }

    clicked.x.push_back(x);
    clicked.y.push_back(y);
    clicked.num++;

    if (verbose)
        printf("DEBUG: Adding click %i (X=%i, Y=%i)\n", clicked.num-1, x, y);

    return true;
}

inline bool Calibrator::along_axis(int xy, int x0, int y0)
{
    return ((abs(xy - x0) <= threshold_misclick) ||
            (abs(xy - y0) <= threshold_misclick));
}

bool Calibrator::finish(int width, int height)
{
    if (get_numclicks() != NUM_POINTS) {
        return false;
    }

    // new axis origin and scaling
    // based on old_axys: inversion/swapping is relative to the old axis
    XYinfo new_axis(old_axys);


    // calculate average of clicks
    float x_min = (clicked.x[UL] + clicked.x[LL])/2.0;
    float x_max = (clicked.x[UR] + clicked.x[LR])/2.0;
    float y_min = (clicked.y[UL] + clicked.y[UR])/2.0;
    float y_max = (clicked.y[LL] + clicked.y[LR])/2.0;

    // Should x and y be swapped?
    if (abs(clicked.x[UL] - clicked.x[UR]) < abs(clicked.y[UL] - clicked.y[UR])) {
        new_axis.swap_xy = !new_axis.swap_xy;
        std::swap(x_min, y_min);
        std::swap(x_max, y_max);
    }

    // the screen was divided in num_blocks blocks, and the touch points were at
    // one block away from the true edges of the screen.
    const float block_x = width/(float)num_blocks;
    const float block_y = height/(float)num_blocks;
    // rescale these blocks from the range of the drawn touchpoints to the range of the 
    // actually clicked coordinates, and substract/add from the clicked coordinates
    // to obtain the coordinates corresponding to the edges of the screen.
    float scale_x = (x_max - x_min)/(width - 2*block_x);
    x_min -= block_x * scale_x;
    x_max += block_x * scale_x;
    float scale_y = (y_max - y_min)/(height - 2*block_y);
    y_min -= block_y * scale_y;
    y_max += block_y * scale_y;
    
    // now, undo the transformations done by the X server, to obtain the true 'raw' value in X.
    // The raw value was scaled from old_axis to the device min/max, and from the device min/max
    // to the screen min/max
    // hence, the reverse transformation is from screen to old_axis
    x_min = scaleAxis(x_min, old_axys.x.max, old_axys.x.min, width, 0);
    x_max = scaleAxis(x_max, old_axys.x.max, old_axys.x.min, width, 0);
    y_min = scaleAxis(y_min, old_axys.y.max, old_axys.y.min, height, 0);
    y_max = scaleAxis(y_max, old_axys.y.max, old_axys.y.min, height, 0);


    // round and put in new_axis struct
    new_axis.x.min = round(x_min); new_axis.x.max = round(x_max);
    new_axis.y.min = round(y_min); new_axis.y.max = round(y_max);

    // finish the data, driver/calibrator specific
    return finish_data(new_axis);
}

const char* Calibrator::get_sysfs_name()
{
    if (is_sysfs_name(device_name))
        return device_name;

    // TODO: more mechanisms

    return NULL;
}

bool Calibrator::is_sysfs_name(const char* name) {
    const char* SYSFS_INPUT="/sys/class/input";
    const char* SYSFS_DEVNAME="device/name";

    DIR* dp = opendir(SYSFS_INPUT);
    if (dp == NULL)
        return false;

    while (dirent* ep = readdir(dp)) {
        if (strncmp(ep->d_name, "event", strlen("event")) == 0) {
            // got event name, get its sysfs device name
            char filename[40]; // actually 35, but hey...
            (void) sprintf(filename, "%s/%s/%s", SYSFS_INPUT, ep->d_name, SYSFS_DEVNAME);

            std::ifstream ifile(filename);
            if (ifile.is_open()) {
                if (!ifile.eof()) {
                    std::string devname;
                    std::getline(ifile, devname);
                    if (devname == name) {
                        if (verbose)
                            printf("DEBUG: Found that '%s' is a sysfs name.\n", name);
                        return true;
                    }
                }
                ifile.close();
            }
        }
    }
    (void) closedir(dp);

    if (verbose)
        printf("DEBUG: Name '%s' does not match any in '%s/event*/%s'\n",
                    name, SYSFS_INPUT, SYSFS_DEVNAME);
    return false;
}

bool Calibrator::has_xorgconfd_support(Display* dpy) {
    bool has_support = false;

    Display* display = dpy;
    if (dpy == NULL) // no connection to reuse
        display = XOpenDisplay(NULL);

    if (display == NULL) {
        fprintf(stderr, "Unable to connect to X server\n");
        exit(1);
    }

    if (strstr(ServerVendor(display), "X.Org") &&
        VendorRelease(display) >= 10800000) {
        has_support = true;
    }

    if (dpy == NULL) // no connection to reuse
        XCloseDisplay(display);

    return has_support;
}

/*
 * FROM xf86Xinput.c
 *
 * Cx     - raw data from touch screen
 * to_max - scaled highest dimension
 *          (remember, this is of rows - 1 because of 0 origin)
 * to_min  - scaled lowest dimension
 * from_max - highest raw value from touch screen calibration
 * from_min  - lowest raw value from touch screen calibration
 *
 * This function is the same for X or Y coordinates.
 * You may have to reverse the high and low values to compensate for
 * different orgins on the touch screen vs X.
 *
 * e.g. to scale from device coordinates into screen coordinates, call
 * xf86ScaleAxis(x, 0, screen_width, dev_min, dev_max);
 */
int
xf86ScaleAxis(int Cx, int to_max, int to_min, int from_max, int from_min)
{
    int X;
    int64_t to_width = to_max - to_min;
    int64_t from_width = from_max - from_min;

    if (from_width) {
        X = (int) (((to_width * (Cx - from_min)) / from_width) + to_min);
    }
    else {
        X = 0;
        printf("Divide by Zero in xf86ScaleAxis\n");
        exit(1);
    }

    if (X > to_max)
        X = to_max;
    if (X < to_min)
        X = to_min;

    return X;
}

// same but without rounding to min/max
float
scaleAxis(float Cx, int to_max, int to_min, int from_max, int from_min)
{
    float X;
    int64_t to_width = to_max - to_min;
    int64_t from_width = from_max - from_min;

    if (from_width) {
        X = (((to_width * (Cx - from_min)) / from_width) + to_min);
    }
    else {
        X = 0;
        printf("Divide by Zero in scaleAxis\n");
        exit(1);
    }

    /* no rounding to max/min
    if (X > to_max)
        X = to_max;
    if (X < to_min)
        X = to_min;
    */

    return X;
}
