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
#include <stdarg.h>

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
                    trace ( "Not adding click %i (X=%i, Y=%i): within %i pixels of previous click\n",
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
                } else {
                    trace ( "Mis-click detected, click %i (X=%i, Y=%i) not aligned with click 0 (X=%i, Y=%i) (threshold=%i)\n",
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
                } else {
                    trace ( "Mis-click detected, click %i (X=%i, Y=%i) not aligned with click 0 (X=%i, Y=%i) or click 1 (X=%i, Y=%i) (threshold=%i)\n",
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
                } else {
                    trace ( "Mis-click detected, click %i (X=%i, Y=%i) not aligned with click 1 (X=%i, Y=%i) or click 2 (X=%i, Y=%i) (threshold=%i)\n",
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

    trace("Adding click %i (X=%i, Y=%i)\n", clicked.num-1, x, y);
    return true;
}

inline bool Calibrator::along_axis(int xy, int x0, int y0)
{
    return ((abs(xy - x0) <= threshold_misclick) ||
            (abs(xy - y0) <= threshold_misclick));
}

/// Compute calibration on 1 axis
/// (all +0.5 for float to int rounding)
void Calibrator::process_axys( int screen_dim, const AxisInfo &previous, std::vector<int> &clicked, AxisInfo &updated )
{
    // These are scaled using the values of old_axys
    const float old_scale = (previous.max - previous.min)/(float)screen_dim;

    // Sort to get lowest two and highest two whatever is the orientation
    std::sort( clicked.begin(), clicked.end());
    // If inverted, must undo inversion since calibration is before in evdev driver.
    if ( previous.invert ) {
        updated.min = ( (2*screen_dim - clicked[2] - clicked[3]) * old_scale/2 ) + previous.min + 0.5;
        updated.max = ( (2*screen_dim - clicked[0] - clicked[1]) * old_scale/2 ) + previous.min + 0.5;
    } else {
        updated.min = ( (clicked[0] + clicked[1]) * old_scale/2 ) + previous.min + 0.5;
        updated.max = ( (clicked[2] + clicked[3]) * old_scale/2 ) + previous.min + 0.5;
    }

    // Add/subtract the offset that comes from not having the points in the
    // corners (using the new scale, assumed better than previous)
    const int new_delta = (updated.max - updated.min) / (float)(num_blocks - 2) + 0.5;
    updated.min -= new_delta;
    updated.max += new_delta;
}

// Compute calibration and correct orientation from captured coordinates
bool Calibrator::finish(int width, int height)
{
    if (get_numclicks() != NUM_POINTS) {
        return false;
    }

    trace ( "Screen size=%dx%d\n", width, height );
    trace ( "Expected screen coordinates, x.min=%d, x.max=%d, y.min=%d, y.max=%d\n",
            width/num_blocks, width-width/num_blocks,
            height/num_blocks, height - height/num_blocks);

    // Evdev v2.3.2 order to compute coordinates from peripheral to screen:
    // - swap xy axis
    // - calibration (offset and scale)
    // - invert x, invert y axis

    trace ( "Previous orientation: swap_xy=%d, invert_x=%d, invert_y=%d\n", old_axys.swap_xy, old_axys.x.invert, old_axys.y.invert );

    // Compute orientation modifications from existing (if orientation is already corrected, no change)
    // Start reverse order vs. evdev: from screen to peripheral

    // Check if axes are inverted ?
    bool invert_x = false, invert_y = false;
    if ( clicked.x[UL] > clicked.x[LR] ) {
        invert_x = true;
    }

    if ( clicked.y[UL] > clicked.y[LR] ) {
        invert_y = true;
    }

    XYinfo new_axys; // new axys origin and scaling

    // Should x and y be swapped
    const bool swap_xy = (abs (clicked.x [UL] - clicked.x [UR]) < abs (clicked.y [UL] - clicked.y [UR]));
    trace ( "Orientation modifications: swap_xy=%d, invert_x=%d, invert_y=%d\n", swap_xy, invert_x, invert_y);

    /// Compute calibration
    if (swap_xy) {
        process_axys( height, old_axys.x, clicked.y, new_axys.x );
        process_axys( width, old_axys.y, clicked.x, new_axys.y );
    } else {
        process_axys( width, old_axys.x, clicked.x, new_axys.x );
        process_axys( height, old_axys.y, clicked.y, new_axys.y );
    }

    new_axys.swap_xy = old_axys.swap_xy ^ swap_xy;
    new_axys.x.invert = old_axys.x.invert ^ invert_x;
    new_axys.y.invert = old_axys.y.invert ^ invert_y;

    trace ( "New orientation: swap_xy=%d, invert_x=%d, invert_y=%d\n", new_axys.swap_xy, new_axys.x.invert, new_axys.y.invert );

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
                        trace("Found that '%s' is a sysfs name.\n", name);
                        return true;
                    }
                }
                ifile.close();
            }
        }
    }
    (void) closedir(dp);

    trace ("Name '%s' does not match any in '%s/event*/%s'\n",
            name, SYSFS_INPUT, SYSFS_DEVNAME);
    return false;
}

bool Calibrator::has_xorgconfd_support(Display* dpy) {
    bool has_support = false;

    Display* display = dpy;
    if (dpy == NULL) // no connection to reuse
        display = XOpenDisplay(NULL);

    if (display == NULL) {
        error ( "Unable to connect to X server\n");
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

/// Write calibration output (to stdout)
void Calibrator::output ( const char *format, ... )
{
    va_list vl;
    va_start ( vl, format );
    vprintf ( format, vl );
    va_end ( vl );
}

/// Dump debug information if verbose activated
void Calibrator::trace ( const char *format, ...)
{
    if ( verbose == true ) {
        printf ( "DEBUG: ");
        va_list vl;
        va_start ( vl, format );
        vprintf ( format, vl );
        va_end ( vl );
    }
}

/// Information to user, if verbose mode activated
void Calibrator::info ( const char *format, ... )
{
    if ( verbose == true ) {
        printf ( "INFO: ");
        va_list vl;
        va_start ( vl, format );
        vprintf ( format, vl );
        va_end ( vl );
    }
}

/// Error (non fatal)
void Calibrator::error ( const char *format, ...)
{
    fprintf ( stderr, "ERROR: ");
    va_list vl;
    va_start ( vl, format );
    vprintf ( format, vl );
    va_end ( vl );
}
