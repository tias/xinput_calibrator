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

#ifndef _calibrator_hh
#define _calibrator_hh

#include <stdexcept>
#include <X11/Xlib.h>

/*
 * Number of blocks. We partition the screen into 'num_blocks' x 'num_blocks'
 * rectangles of equal size. We then ask the user to press points that are
 * located at the corner closes to the center of the four blocks in the corners
 * of the screen. The following ascii art illustrates the situation. We partition
 * the screen into 8 blocks in each direction. We then let the user press the
 * points marked with 'O'.
 *
 *   +--+--+--+--+--+--+--+--+
 *   |  |  |  |  |  |  |  |  |
 *   +--O--+--+--+--+--+--O--+
 *   |  |  |  |  |  |  |  |  |
 *   +--+--+--+--+--+--+--+--+
 *   |  |  |  |  |  |  |  |  |
 *   +--+--+--+--+--+--+--+--+
 *   |  |  |  |  |  |  |  |  |
 *   +--+--+--+--+--+--+--+--+
 *   |  |  |  |  |  |  |  |  |
 *   +--+--+--+--+--+--+--+--+
 *   |  |  |  |  |  |  |  |  |
 *   +--+--+--+--+--+--+--+--+
 *   |  |  |  |  |  |  |  |  |
 *   +--O--+--+--+--+--+--O--+
 *   |  |  |  |  |  |  |  |  |
 *   +--+--+--+--+--+--+--+--+
 */
const int num_blocks = 8;

/// struct to hold min/max info of the X and Y axis
struct XYinfo {
    int x_min;
    int x_max;
    int y_min;
    int y_max;
    XYinfo() : x_min(-1), x_max(-1), y_min(-1), y_max(-1) {}
    XYinfo(int xmi, int xma, int ymi, int yma) :
         x_min(xmi), x_max(xma), y_min(ymi), y_max(yma) {}
};

/// Names of the points
enum {
    UL = 0, // Upper-left
    UR = 1, // Upper-right
    LL = 2, // Lower-left
    LR = 3,  // Lower-right
    NUM_POINTS
};

/// Output types
enum OutputType {
    OUTYPE_AUTO,
    OUTYPE_XORGCONFD,
    OUTYPE_HAL,
    OUTYPE_XINPUT
};

class WrongCalibratorException : public std::invalid_argument {
    public:
        WrongCalibratorException(const std::string& msg = "") :
            std::invalid_argument(msg) {}
};

// Abstract base class for calculating new calibration parameters
class Calibrator
{
public:
    /// Parse arguments and create calibrator
    static Calibrator* make_calibrator(int argc, char** argv);

    /* Constructor for a specific calibrator
     *
     * The constructor will throw an exception,
     * if the touchscreen is not of the type it supports
     */
    Calibrator(const char* const device_name, const XYinfo& axys,
        const bool verbose, const int thr_misclick=0, const int thr_doubleclick=0, const OutputType output_type=OUTYPE_AUTO, const char* geometry=0);
    ~Calibrator() {}

    // set the doubleclick treshold
    void set_threshold_doubleclick(int t);
    // set the misclick treshold
    void set_threshold_misclick(int t);
    // get the number of clicks already registered
    int get_numclicks();
    // return geometry string or NULL
    const char* get_geometry();
    // reset clicks
    void reset() {
        num_clicks = 0;
    }
    // add a click with the given coordinates
    bool add_click(int x, int y);
    // calculate and apply the calibration
    bool finish(int width, int height);
    // get the sysfs name of the device,
    // returns NULL if it can not be found
    const char* get_sysfs_name();

protected:
    // check whether the coordinates are along the respective axis
    bool along_axis(int xy, int x0, int y0);

    // overloaded function that applies the new calibration
    virtual bool finish_data(const XYinfo new_axys, int swap_xy) =0;

    // name of the device (driver)
    const char* const device_name;
    // original axys values
    XYinfo old_axys;
    // be verbose or not
    bool verbose;
    // nr of clicks registered
    int num_clicks;
    // click coordinates
    int clicked_x[NUM_POINTS], clicked_y[NUM_POINTS];

    // Threshold to keep the same point from being clicked twice.
    // Set to zero if you don't want this check
    int threshold_doubleclick;

    // Threshold to detect mis-clicks (clicks not along axes)
    // A lower value forces more precise calibration
    // Set to zero if you don't want this check
    int threshold_misclick;

    // Type of output
    OutputType output_type;

    // manually specified geometry string
    const char* geometry;

    // Check whether the given name is a sysfs device name
    bool is_sysfs_name(const char* name);

    // Check whether the X server has xorg.conf.d support
    bool has_xorgconfd_support(Display* display=NULL);
};

#endif
