/*
 * Copyright (c) 2009 Tias Guns
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
#include <algorithm>
#include <sys/types.h>
#include <dirent.h>
#include <iostream>
#include <fstream>
#include <string>

#include "calibrator.hh"

Calibrator::Calibrator(const char* const device_name0, const XYinfo& axys0,
    const bool verbose0, const int thr_misclick, const int thr_doubleclick, const OutputType output_type0)
  : device_name(device_name0), old_axys(axys0), verbose(verbose0), num_clicks(0), threshold_doubleclick(thr_doubleclick), threshold_misclick(thr_misclick), output_type(output_type0)
{
}

void Calibrator::set_threshold_doubleclick(int t)
{
    threshold_doubleclick = t;
}

void Calibrator::set_threshold_misclick(int t)
{
    threshold_misclick = t;
}

int Calibrator::get_numclicks()
{
    return num_clicks;
}

bool Calibrator::add_click(int x, int y)
{
    // Double-click detection
    if (threshold_doubleclick > 0 && num_clicks > 0) {
        int i = num_clicks-1;
        while (i >= 0) {
            if (abs(x - clicked_x[i]) <= threshold_doubleclick
                && abs(y - clicked_y[i]) <= threshold_doubleclick) {
                if (verbose) {
                    printf("DEBUG: Not adding click %i (X=%i, Y=%i): within %i pixels of previous click\n",
                        num_clicks, x, y, threshold_doubleclick);
                }
                return false;
            }
            i--;
        }
    }

    // Mis-click detection
    if (threshold_misclick > 0 && num_clicks > 0) {
        bool misclick = true;

        if (num_clicks == 1) {
            // check that along one axis of first point
            if (along_axis(x,clicked_x[0],clicked_y[0]) ||
                along_axis(y,clicked_x[0],clicked_y[0]))
                misclick = false;
        } else if (num_clicks == 2) {
            // check that along other axis of first point than second point
            if ((along_axis(y,clicked_x[0],clicked_y[0]) &&
                 along_axis(clicked_x[1],clicked_x[0],clicked_y[0])) ||
                (along_axis(x,clicked_x[0],clicked_y[0]) &&
                 along_axis(clicked_y[1],clicked_x[0],clicked_y[0])))
                misclick = false;
        } else if (num_clicks == 3) {
            // check that along both axis of second and third point
            if ((along_axis(x,clicked_x[1],clicked_y[1]) &&
                 along_axis(y,clicked_x[2],clicked_y[2])) ||
                (along_axis(y,clicked_x[1],clicked_y[1]) &&
                 along_axis(x,clicked_x[2],clicked_y[2])))
                misclick = false;
        }

        if (misclick) {
            if (verbose) {
                if (num_clicks == 1)
                    printf("DEBUG: Mis-click detected, click %i (X=%i, Y=%i) not aligned with click 0 (X=%i, Y=%i) (threshold=%i)\n", num_clicks, x, y, clicked_x[0], clicked_y[0], threshold_misclick);
                else if (num_clicks == 2)
                    printf("DEBUG: Mis-click detected, click %i (X=%i, Y=%i) not aligned with click 0 (X=%i, Y=%i) or click 1 (X=%i, Y=%i) (threshold=%i)\n", num_clicks, x, y, clicked_x[0], clicked_y[0], clicked_x[1], clicked_y[1], threshold_misclick);
                else if (num_clicks == 3)
                    printf("DEBUG: Mis-click detected, click %i (X=%i, Y=%i) not aligned with click 1 (X=%i, Y=%i) or click 2 (X=%i, Y=%i) (threshold=%i)\n", num_clicks, x, y, clicked_x[1], clicked_y[1], clicked_x[2], clicked_y[2], threshold_misclick);
            }

            reset();
            return false;
        }
    }

    clicked_x[num_clicks] = x;
    clicked_y[num_clicks] = y;
    num_clicks++;

    if (verbose)
        printf("DEBUG: Adding click %i (X=%i, Y=%i)\n", num_clicks-1, x, y);

    return true;
}

inline bool Calibrator::along_axis(int xy, int x0, int y0)
{
    return ((abs(xy - x0) <= threshold_misclick) ||
            (abs(xy - y0) <= threshold_misclick));
}

bool Calibrator::finish(int width, int height)
{
    if (get_numclicks() != 4) {
        return false;
    }

    // Should x and y be swapped?
    const bool swap_xy = (abs (clicked_x [UL] - clicked_x [UR]) < abs (clicked_y [UL] - clicked_y [UR]));
    if (swap_xy) {
        std::swap(clicked_x[LL], clicked_x[UR]);
        std::swap(clicked_y[LL], clicked_y[UR]);
    }

    // Compute min/max coordinates.
    XYinfo axys;
    // These are scaled using the values of old_axys
    const float scale_x = (old_axys.x_max - old_axys.x_min)/(float)width;
    axys.x_min = ((clicked_x[UL] + clicked_x[LL]) * scale_x/2) + old_axys.x_min;
    axys.x_max = ((clicked_x[UR] + clicked_x[LR]) * scale_x/2) + old_axys.x_min;
    const float scale_y = (old_axys.y_max - old_axys.y_min)/(float)height;
    axys.y_min = ((clicked_y[UL] + clicked_y[UR]) * scale_y/2) + old_axys.y_min;
    axys.y_max = ((clicked_y[LL] + clicked_y[LR]) * scale_y/2) + old_axys.y_min;

    // Add/subtract the offset that comes from not having the points in the
    // corners (using the same coordinate system they are currently in)
    const int delta_x = (axys.x_max - axys.x_min) / (float)(num_blocks - 2);
    axys.x_min -= delta_x;
    axys.x_max += delta_x;
    const int delta_y = (axys.y_max - axys.y_min) / (float)(num_blocks - 2);
    axys.y_min -= delta_y;
    axys.y_max += delta_y;


    // If x and y has to be swapped we also have to swap the parameters
    if (swap_xy) {
        std::swap(axys.x_min, axys.y_max);
        std::swap(axys.y_min, axys.x_max);
    }

    // finish the data, driver/calibrator specific
    return finish_data(axys, swap_xy);
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
