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

#include "calibrator.hh"

Calibrator::Calibrator(const char* const device_name0, const XYinfo& axys0,
    const bool verbose0, const int thr_misclick, const int thr_doubleclick, const OutputType output_type0, const char* geometry0)
  : device_name(device_name0), verbose(verbose0), threshold_doubleclick(thr_doubleclick), threshold_misclick(thr_misclick), output_type(output_type0), geometry(geometry0)
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

    XYinfo new_axys; // new axys origin and scaling

    // Should x and y be swapped?
    new_axys.swap_xy = (abs (clicked.x[UL] - clicked.x[UR]) < abs (clicked.y[UL] - clicked.y[UR]));

    if (new_axys.swap_xy) {
        std::swap(clicked.x[LL], clicked.x[UR]);
        std::swap(clicked.y[LL], clicked.y[UR]);
    }

    // Compute min/max coordinates.
    // These are scaled using the values of old_axys
    const float scale_x = (old_axys.x.max - old_axys.x.min)/(float)width;
    new_axys.x.min = ((clicked.x[UL] + clicked.x[LL]) * scale_x/2) + old_axys.x.min;
    new_axys.x.max = ((clicked.x[UR] + clicked.x[LR]) * scale_x/2) + old_axys.x.min;
    const float scale_y = (old_axys.y.max - old_axys.y.min)/(float)height;
    new_axys.y.min = ((clicked.y[UL] + clicked.y[UR]) * scale_y/2) + old_axys.y.min;
    new_axys.y.max = ((clicked.y[LL] + clicked.y[LR]) * scale_y/2) + old_axys.y.min;

    // Add/subtract the offset that comes from not having the points in the
    // corners (using the same coordinate system they are currently in)
    const int delta_x = (new_axys.x.max - new_axys.x.min) / (float)(num_blocks - 2);
    new_axys.x.min -= delta_x;
    new_axys.x.max += delta_x;
    const int delta_y = (new_axys.y.max - new_axys.y.min) / (float)(num_blocks - 2);
    new_axys.y.min -= delta_y;
    new_axys.y.max += delta_y;


    // If x and y has to be swapped we also have to swap the parameters
    if (new_axys.swap_xy) {
        std::swap(new_axys.x.min, new_axys.y.max);
        std::swap(new_axys.y.min, new_axys.x.max);
    }

    // finish the data, driver/calibrator specific
    return finish_data(new_axys);
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
