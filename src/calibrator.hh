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

#ifndef _calibrator_hh
#define _calibrator_hh

#include <stdexcept>
#include <X11/Xlib.h>
#include <stdio.h>
#include <vector>

// XXX: we currently don't handle lines that are longer than this
#define MAX_LINE_LEN 1024

int xf86ScaleAxis(int Cx, int to_max, int to_min, int from_max, int from_min);
float scaleAxis(float Cx, int to_max, int to_min, int from_max, int from_min);

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

struct AxisInfo {
    int min, max;
    bool invert;

    AxisInfo() : min(-1), max(-1), invert(false) { }
    AxisInfo(int mi, int ma, bool inv = false) :
        min(mi), max(ma), invert(inv) { }
    AxisInfo(const AxisInfo& old) :
        min(old.min), max(old.max), invert(old.invert) { }

    void do_invert() {
        invert = !invert;
    }
};

/// struct to hold min/max info of the X and Y axis
struct XYinfo {
    /// Axis swapped
    bool swap_xy;
    /// X, Y axis
    AxisInfo x, y;

    XYinfo() : swap_xy(false) {}

    XYinfo(int xmi, int xma, int ymi, int yma, bool swap_xy_ = false,
        bool inv_x = false, bool inv_y = false) :
        swap_xy(swap_xy_), x(xmi, xma, inv_x), y(ymi, yma, inv_y) {}

    XYinfo(const XYinfo& old) :
        swap_xy(old.swap_xy), x(old.x), y(old.y) {}

    void do_xf86ScaleAxis(const XYinfo& to, const XYinfo& from) {
        x.min = xf86ScaleAxis(x.min, to.x.max, to.x.min, from.x.max, from.x.min);
        x.max = xf86ScaleAxis(x.max, to.x.max, to.x.min, from.x.max, from.x.min);
        y.min = xf86ScaleAxis(y.min, to.y.max, to.y.min, from.y.max, from.y.min);
        y.max = xf86ScaleAxis(y.max, to.y.max, to.y.min, from.y.max, from.y.min);
    }

    void print(const char* xtra="\n") {
        printf("XYinfo: x.min=%i, x.max=%i, y.min=%i, y.max=%i, swap_xy=%i, invert_x=%i, invert_y=%i%s",
               x.min, x.max, y.min, y.max, swap_xy, x.invert, y.invert, xtra);
    }
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

/// Base class for calculating new calibration parameters
class Calibrator
{
public:
    /// Parse arguments and create calibrator
    static Calibrator* make_calibrator(int argc, char** argv);

    /// Constructor
    ///
    /// The constructor will throw an exception,
    /// if the touchscreen is not of the type it supports
    Calibrator(const char* const device_name,
               const XYinfo& axys,
               const int thr_misclick=0,
               const int thr_doubleclick=0,
               const OutputType output_type=OUTYPE_AUTO,
               const char* geometry=0,
               const bool use_timeout=1,
               const char* output_filename = 0);

    virtual ~Calibrator() {}

    /// set the doubleclick treshold
    void set_threshold_doubleclick(int t)
    { threshold_doubleclick = t; }

    /// set the misclick treshold
    void set_threshold_misclick(int t)
    { threshold_misclick = t; }

    /// get the number of clicks already registered
    int get_numclicks() const
    { return clicked.num; }

    /// return geometry string or NULL
    const char* get_geometry() const
    { return geometry; }

    /// reset clicks
    void reset()
    {  clicked.num = 0; clicked.x.clear(); clicked.y.clear();}

    /// add a click with the given coordinates
    bool add_click(int x, int y);
    /// calculate and apply the calibration
    virtual bool finish(int width, int height);
    /// get the sysfs name of the device,
    /// returns NULL if it can not be found
    const char* get_sysfs_name();

    const bool get_use_timeout() const
    { return use_timeout; }

    /// get output filename set at cmdline or NULL
    const char* get_output_filename() const
    { return output_filename; }

protected:
    /// check whether the coordinates are along the respective axis
    bool along_axis(int xy, int x0, int y0);

    /// Apply new calibration, implementation dependent
    virtual bool finish_data(const XYinfo new_axys) =0;

    /// Check whether the given name is a sysfs device name
    bool is_sysfs_name(const char* name);

    /// Check whether the X server has xorg.conf.d support
    bool has_xorgconfd_support(Display* display=NULL);

    static int find_device(const char* pre_device, bool list_devices,
            XID& device_id, const char*& device_name, XYinfo& device_axys);

protected:
    /// Name of the device (driver)
    const char* const device_name;

    /// Original values
    XYinfo old_axys;

    /// Be verbose or not
    static bool verbose;

    /// Clicked values (screen coordinates)
    struct {
        /// actual number of clicks registered
        int num;
        /// click coordinates
        std::vector<int> x, y;
    } clicked;

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

    const bool use_timeout;

    // manually specified output filename
    const char* output_filename;

    // sysfs path/file
    static const char* SYSFS_INPUT;
    static const char* SYSFS_DEVNAME;
};

// Interfance for a CalibratorTester
class CalibratorTesterInterface
{
public:
    // emulate the driver processing the coordinates in 'raw'
    virtual XYinfo emulate_driver(const XYinfo& raw, bool useNewAxis, const XYinfo& screen, const XYinfo& device) = 0;

    virtual void new_axis_print() = 0;

    //* From Calibrator
    /// add a click with the given coordinates
    virtual bool add_click(int x, int y) = 0;
    /// calculate and apply the calibration
    virtual bool finish(int width, int height) = 0;
};

#endif
