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
    const bool verbose0, const int thr_misclick, const int thr_doubleclick, const OutputType output_type0, const char* geometry0)
  : device_name(device_name0), old_axys(axys0), verbose(verbose0), num_clicks(0), threshold_doubleclick(thr_doubleclick), threshold_misclick(thr_misclick), output_type(output_type0), geometry(geometry0)
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

const char* Calibrator::get_geometry()
{
    return geometry;
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

static int scale_value(int val, int src_min, int src_max, int dst_min, int dst_max)
{
    int new_val;
    const int src_range = src_max - src_min;
    const int dst_range = dst_max - dst_min;
    const float scale = (float)dst_range / (float)src_range;
    new_val = (scale * (float)(val - src_min)) + dst_min;
    if (new_val > dst_max)
        new_val = dst_max;
    else if (new_val < dst_min)
        new_val = dst_min;
    return new_val;
}

bool Calibrator::finish(int width, int height)
{
    if (get_numclicks() != 4) {
        return false;
    }

    if (verbose) {
        printf("DEBUG: Screen: %dx%d\n", width, height);
        printf("DEBUG: Upper Left:  (%u, %u)\n", clicked_x[UL], clicked_y[UL]);
        printf("DEBUG: Upper Right: (%u, %u)\n", clicked_x[UR], clicked_y[UR]);
        printf("DEBUG: Lower Left:  (%u, %u)\n", clicked_x[LL], clicked_y[LL]);
        printf("DEBUG: Lower Right: (%u, %u)\n", clicked_x[LR], clicked_y[LR]);
    }

    // Convert from screen coordinates to device coordinates.
    clicked_x[UL] = scale_value(clicked_x[UL], 0, width, old_axys.x_min, old_axys.x_max);
    clicked_x[UR] = scale_value(clicked_x[UR], 0, width, old_axys.x_min, old_axys.x_max);
    clicked_x[LL] = scale_value(clicked_x[LL], 0, width, old_axys.x_min, old_axys.x_max);
    clicked_x[LR] = scale_value(clicked_x[LR], 0, width, old_axys.x_min, old_axys.x_max);

    clicked_y[UL] = scale_value(clicked_y[UL], 0, height, old_axys.y_min, old_axys.y_max);
    clicked_y[UR] = scale_value(clicked_y[UR], 0, height, old_axys.y_min, old_axys.y_max);
    clicked_y[LL] = scale_value(clicked_y[LL], 0, height, old_axys.y_min, old_axys.y_max);
    clicked_y[LR] = scale_value(clicked_y[LR], 0, height, old_axys.y_min, old_axys.y_max);

    if (verbose) {
        printf("DEBUG: After conversion to device coordinates:\n");
        printf("DEBUG: Upper Left:  (%u, %u)\n", clicked_x[UL], clicked_y[UL]);
        printf("DEBUG: Upper Right: (%u, %u)\n", clicked_x[UR], clicked_y[UR]);
        printf("DEBUG: Lower Left:  (%u, %u)\n", clicked_x[LL], clicked_y[LL]);
        printf("DEBUG: Lower Right: (%u, %u)\n", clicked_x[LR], clicked_y[LR]);
    }

    // Should x and y be swapped?
    const bool swap_xy = (abs(clicked_x[UL] - clicked_x[UR]) < abs(clicked_y[UL] - clicked_y[UR]));
    if (swap_xy) {
        if (verbose)
            printf("DEBUG: Axes are swapped.\n");
        std::swap(clicked_x[UL], clicked_y[UL]);
        std::swap(clicked_x[UR], clicked_y[UR]);
        std::swap(clicked_x[LL], clicked_y[LL]);
        std::swap(clicked_x[LR], clicked_y[LR]);
    }

    if (verbose) {
        printf("DEBUG: After swapping:\n");
        printf("DEBUG: Upper Left:  (%u, %u)\n", clicked_x[UL], clicked_y[UL]);
        printf("DEBUG: Upper Right: (%u, %u)\n", clicked_x[UR], clicked_y[UR]);
        printf("DEBUG: Lower Left:  (%u, %u)\n", clicked_x[LL], clicked_y[LL]);
        printf("DEBUG: Lower Right: (%u, %u)\n", clicked_x[LR], clicked_y[LR]);
    }

    XYinfo axys;

    // Calculate average values for min/max.
    axys.x_min = ((float)(clicked_x[UL] + clicked_x[LL]) / 2.0);
    axys.x_max = ((float)(clicked_x[UR] + clicked_x[LR]) / 2.0);
    axys.y_min = ((float)(clicked_y[UL] + clicked_y[UR]) / 2.0);
    axys.y_max = ((float)(clicked_y[LL] + clicked_y[LR]) / 2.0);

    if (verbose)
        printf("DEBUG: Averaged values [%u:%u], [%u:%u]:\n",
               axys.x_min, axys.x_max, axys.y_min, axys.y_max);

    // Scale values out to the actual screen corners.
    float scale = 1.0 / (float)(num_blocks - 2);
    int x_delta = scale * (float)(axys.x_max - axys.x_min);
    axys.x_min -= x_delta;
    axys.x_max += x_delta;
    int y_delta = scale * (float)(axys.y_max - axys.y_min);
    axys.y_min -= y_delta;
    axys.y_max += y_delta;

    if (verbose)
        printf("DEBUG: Final values [%u:%u], [%u:%u]:\n",
               axys.x_min, axys.x_max, axys.y_min, axys.y_max);

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
