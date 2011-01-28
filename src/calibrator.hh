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
#include <vector>

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
};

/// struct to hold min/max info of the X and Y axis
struct XYinfo {
  /// Axis swapped
  bool swap_xy;
  /// X, Y axis
  AxisInfo x, y;
  
  XYinfo() : swap_xy(false) {}
  
  XYinfo(int xmi, int xma, int ymi, int yma, bool swap_xy_ = false):
  swap_xy(swap_xy_) {  x.min = xmi; x.max = xma; y.min = ymi; y.max = yma;}
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
			   const char* geometry=0);
			   
	~Calibrator() 
	{}

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
    bool finish(int width, int height);
    /// get the sysfs name of the device,
    /// returns NULL if it can not be found
    const char* get_sysfs_name();

protected:
    /// check whether the coordinates are along the respective axis
    bool along_axis(int xy, int x0, int y0);

    /// Apply new calibration, implementation dependent
	///\param[in] swap_xy if true, X and Y axes are swapped
	///\param[in] invert_x if true, X axis is inverted
	///\param[in] invert_y if true, Y axis is inverted
    virtual bool finish_data(const XYinfo new_axys ) =0;
	
	/// Compute calibration on 1 axis
	void process_axys( int screen_dim, const AxisInfo &previous, std::vector<int> &clicked, AxisInfo &updated );
	
	/// Check whether the given name is a sysfs device name
	bool is_sysfs_name(const char* name);
	
	/// Check whether the X server has xorg.conf.d support
	bool has_xorgconfd_support(Display* display=NULL);
	
	/// Write output calibration (to stdout)
	void output ( const char *format, ... );
	
	/// Write information to user, if verbose mode activated
	static void info ( const char *format, ... );
	
	/// Trace debug information if verbose activated
	static void trace ( const char *format, ...);
	
	/// Write error (non fatal)
	static void error ( const char *format, ...);
	
	static int find_device(const char* pre_device, bool list_devices,
						   XID& device_id, const char*& device_name, XYinfo& device_axys);
  protected:
	// be verbose or not
	static bool verbose;
		
    // name of the device (driver)
    const char* const device_name;
	/// Original values
	XYinfo old_axys;
	
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

    /// Format or type of output calibration
    OutputType output_type;

	/// manually specified geometry string for the calibration pattern
	const char* geometry;
};

#endif
