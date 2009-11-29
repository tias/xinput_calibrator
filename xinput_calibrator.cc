/*
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of Red Hat
 * not be used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.  Red
 * Hat makes no representations about the suitability of this software
 * for any purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * THE AUTHORS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors:
 *	Soren Hauberg (haub...@gmail.com)
 *	Tias Guns (ti..@ulyssis.org)
 *
 * Version: 0.2.0
 *
 * Make: g++ -Wall xinput_calibrator.cc `pkg-config --cflags --libs gtkmm-2.4` -o xinput_calibrator
 */

/*
 * This program performs calibration of touchscreens.
 * A calibration consists of finding the following parameters:
 *
 *   'flip_x'
 *      a boolean parameter that determines if the x-coordinate should be flipped.
 *
 *   'flip_y'
 *      same as 'flip_x' except for the y-coordinate.
 *
 *   'swap_xy'
 *      a boolean parameter that determines if the x and y-coordinates should
 *      be swapped.
 *
 *   'min_x', 'max_x', 'min_y', and 'max_y'
 *      the minimum and maximum x and y values that the screen can report. In principle
 *      we could get these parameters by letting the user press the corners of the
 *      screen. In practice this doesn't work, because the screen doesn't always have
 *      sensors at the corners. So, what we do is we ask the user to press points that
 *      have been pushed a bit closer to the center, and then we extrapolate the
 *      parameters.
 *
 * In case of the usbtouchscreen kernel module, the new calibration data will be
 * dynamically updated. In all other cases, the new xorg.conf values will be printed
 * on stdout.
 */

#include <cstring>

#include <gtkmm/main.h>
#include <gtkmm/window.h>
#include <gtkmm/drawingarea.h>
#include <cairomm/context.h>

#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>

// begin EvDev includes
#include <X11/Xatom.h>
//#include <X11/Xutil.h>

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 1
#endif
#ifndef EXIT_FAILURE
#define EXIT_FAILURE 0
#endif
// end EvDev includes

/* Number of points the user should click */
const int num_points = 4;

/* Number of blocks. We partition the screen into 'num_blocks' x 'num_blocks'
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

/* Names of the points */
enum {
  UL = 0, /* Upper-left */
  UR = 1, /* Upper-right */
  LL = 2, /* Lower-left */
  LR = 3  /* Lower-right */
};

/* Threshold to keep the same point from being clicked twice. Set to zero if you don't
 * want this check */
const int click_threshold = 7;

/* Timeout parameters */
const int time_step = 100; /* 500 = 0.1 sec */
const int max_time = 15000; /* 5000 = 5 sec */

/* Clock Appereance */
const int clock_radius = 25;
const int clock_line_width = 10;

/* Text printed on screen */
const int font_size = 20;
const std::string help_text = "Press the point with the stylus";

/*************************
 * Variables for usbtouchscreen specifically
 * should all be put in the CalibratorUsbts class
 *************************/

/* The file to which the calibration parameters are saved. (XXX: is this distribution dependend?) */
const char *modprobe_conf_local = "/etc/modprobe.conf.local";

/* Prefix to the kernel path where we can set the parameters */
const char *module_prefix = "/sys/module/usbtouchscreen/parameters";

/* Names of kernel parameters */
const char *p_range_x = "range_x";
const char *p_range_y = "range_y";
const char *p_min_x = "min_x";
const char *p_min_y = "min_y";
const char *p_max_x = "max_x";
const char *p_max_y = "max_y";
const char *p_transform_xy = "transform_xy";
const char *p_flip_x = "flip_x";
const char *p_flip_y = "flip_y";
const char *p_swap_xy = "swap_xy";



void quit (int status)
{
  try {
    // Gtk may not be in a loop yet?
    Gtk::Main::quit ();
  } catch (...) {
  }
  exit(status);
}


/* Class for calculating new calibration parameters */
class Calibrator
{
public:
  Calibrator (const char*, int, int, int, int);
  ~Calibrator () { }

  bool add_click (double cx, double cy);
  virtual void finish ();
  virtual void finish_data (int, int, int, int, int, int, int) =0;
  void set_width (int w);
  void set_height (int h);

protected:
  double clicked_x [num_points], clicked_y [num_points];
  int num_clicks, width, height;
  const char* drivername;
  /* Old output ranges. */
  int oldcalib_min_x, oldcalib_max_x;
  int oldcalib_min_y, oldcalib_max_y;
};

Calibrator::Calibrator (const char* drivername0,
    int min_x, int max_x, int min_y, int max_y)
  : num_clicks (0), drivername (drivername0), oldcalib_min_x(min_x), oldcalib_max_x(max_x), oldcalib_min_y(min_y), oldcalib_max_y(max_y)
{
}

void Calibrator::set_width (int w)
{
  width = w;
}

void Calibrator::set_height (int h)
{
  height = h;
}

bool Calibrator::add_click (double cx, double cy)
{
  /* Check that we don't click the same point twice */
  if (num_clicks > 0 && click_threshold > 0
  && abs (cx - clicked_x [num_clicks-1]) < click_threshold
  && abs (cy - clicked_y [num_clicks-1]) < click_threshold)
    return false;

  clicked_x [num_clicks] = cx;
  clicked_y [num_clicks] = cy;
  num_clicks ++;

  return true;
}

void Calibrator::finish ()
{
  /* Should x and y be swapped? */
  const bool swap_xy = (abs (clicked_x [UL] - clicked_x [UR]) < abs (clicked_y [UL] - clicked_y [UR]));
  if (swap_xy)
    {
      std::swap (clicked_x [LL], clicked_x [UR]);
      std::swap (clicked_y [LL], clicked_y [UR]);
    }

  /* Compute min/max coordinates.
   * These are scaled to [oldcalib_min, oldcalib_max]] */
  const float scale_x = (oldcalib_max_x - oldcalib_min_x)/(float)width;
  int min_x = ((clicked_x[UL] + clicked_x[LL]) * scale_x/2) + oldcalib_min_x;
  int max_x = ((clicked_x[UR] + clicked_x[LR]) * scale_x/2) + oldcalib_min_x;
  const float scale_y = (oldcalib_max_y - oldcalib_min_y)/(float)height;
  int min_y = ((clicked_y[UL] + clicked_y[UR]) * scale_y/2) + oldcalib_min_y;
  int max_y = ((clicked_y[LL] + clicked_y[LR]) * scale_y/2) + oldcalib_min_y;

  /* Add/subtract the offset that comes from not having the points in the
   * corners (using the same coordinate system they are currently in) */
  const int delta_x = (max_x - min_x) / (float)(num_blocks - 2);
  min_x = min_x - delta_x;
  max_x = max_x + delta_x;
  const int delta_y = (max_y - min_y) / (float)(num_blocks - 2);
  min_y = min_y - delta_y;
  max_y = max_y + delta_y;


  /* Should x and y be flipped? */
  const bool flip_x = (min_x > max_x);
  const bool flip_y = (min_y > max_y);

  /* If x and y has to be swapped we also have to swap the parameters */
  if (swap_xy)
    {
      std::swap (min_x, max_y);
      std::swap (min_y, max_x);
    }

  finish_data(min_x, max_x,
              min_y, max_y,
              swap_xy,
              flip_x, flip_y);
}

/***************************
 * Class for generic driver,
 * outputs new Xorg values (on stdout for now)
 ***************************/
class CalibratorXorgPrint: public Calibrator
{
public:
  CalibratorXorgPrint (const char*, int, int, int, int);

  virtual void finish_data (int, int, int, int, int, int, int);
};

CalibratorXorgPrint::CalibratorXorgPrint (const char* drivername0, int min_x, int max_x, int min_y, int max_y)
  : Calibrator (drivername0, min_x, max_x, min_y, max_y)
{
    printf ("Calibrating Xorg driver \"%s\" (currently having min_x=%d, max_x=%d and min_y=%d, max_y=%d)\n",
                drivername, oldcalib_min_x, oldcalib_max_x, oldcalib_min_y, oldcalib_max_y);
    printf("\tIf the current calibration data is estimated wrong then either supply it manually with --precalib <minx> <maxx> <miny> <maxy> or run the 'get_precalib.sh' script to automatically get it from your current Xorg configuration (through hal).\n");
}

void CalibratorXorgPrint::finish_data (
    int min_x, int max_x, int min_y, int max_y, int swap_xy, int flip_x, int flip_y)
{
  /* We ignore flip_x and flip_y,
   * the min/max values will already be flipped and drivers can handle this */

  // FDI policy file
  printf("\nNew method for making the calibration permanent: create an FDI policy file like /etc/hal/fdi/policy/touchscreen.fdi with:\n\
<match key=\"info.product\" contains=\"%%Name_Of_TouchScreen%%\">\n\
  <merge key=\"input.x11_options.minx\" type=\"string\">%d</merge>\n\
  <merge key=\"input.x11_options.miny\" type=\"string\">%d</merge>\n\
  <merge key=\"input.x11_options.maxx\" type=\"string\">%d</merge>\n\
  <merge key=\"input.x11_options.maxy\" type=\"string\">%d</merge>\n\
</match>\n", min_x, max_x, min_y, max_y);

  printf ("\nOld method, edit /etc/X11/xorg.conf and add in the 'Section \"Device\"' of your touchscreen device:\n");
  printf ("\tOption\t\"MinX\"\t\t\"%d\"\t# was \"%d\"\n",
                min_x, oldcalib_min_x);
  printf ("\tOption\t\"MaxX\"\t\t\"%d\"\t# was \"%d\"\n",
                max_x, oldcalib_max_x);
  printf ("\tOption\t\"MinY\"\t\t\"%d\"\t# was \"%d\"\n",
                min_y, oldcalib_min_y);
  printf ("\tOption\t\"MaxY\"\t\t\"%d\"\t# was \"%d\"\n",
                max_y, oldcalib_max_y);
  if (swap_xy != 0)
    printf ("\tOption\t\"SwapXY\"\t\"%d\"\n", swap_xy);
}



/***************************
 * Class for dynamic evdev calibration
 * uses xinput "Evdev Axis Calibration"
 ***************************/
class CalibratorEvdev: public Calibrator
{
private:
  Display     *display;
  XDeviceInfo *info;
  XDevice     *dev;
public:
  CalibratorEvdev (const char*, int, int, int, int);
  ~CalibratorEvdev ();

  virtual void finish_data (int, int, int, int, int, int, int);

  static Bool check_driver(const char *name);

  // xinput functions
  static Atom parse_atom(Display *display, const char *name);
  static XDeviceInfo* find_device_info(Display *display, const char *name, Bool only_extended);
  int do_set_prop(Display *display, Atom type, int format, int argc, char *argv[]);
};

CalibratorEvdev::CalibratorEvdev (const char* drivername0, int min_x, int max_x, int min_y, int max_y)
  : Calibrator (drivername0, min_x, max_x, min_y, max_y)
{
    Atom                property;
    Atom                act_type;
    int                 act_format;
    unsigned long       nitems, bytes_after;
    unsigned char       *data, *ptr;

    printf ("Calibrating EVDEV driver for \"%s\"\n", drivername);

    // init
    display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "Unable to connect to X server\n");
        return;
    }


    info = find_device_info(display, drivername, False);
    if (!info)
    {
        fprintf(stderr, "unable to find device %s\n", drivername);
        return;
    }

    dev = XOpenDevice(display, info->id);
    if (!dev)
    {
        fprintf(stderr, "unable to open device '%s'\n", info->name);
        return;
    }

    // get "Evdev Axis Calibration" property
    property = parse_atom(display, "Evdev Axis Calibration");
    if (XGetDeviceProperty(display, dev, property, 0, 1000, False,
                           AnyPropertyType, &act_type, &act_format,
                           &nitems, &bytes_after, &data) == Success)
    {
        if (act_format != 32 || act_type != XA_INTEGER) {
            fprintf(stderr, "Error: unexpected format or type from \"Evdev Axis Calibration\" property.\n");
        }

        if (nitems != 0) {
            ptr = data;

            oldcalib_min_x = *((long*)ptr);
            ptr += sizeof(long);
            oldcalib_max_x = *((long*)ptr);
            ptr += sizeof(long);
            oldcalib_min_y = *((long*)ptr);
            ptr += sizeof(long);
            oldcalib_max_y = *((long*)ptr);
            ptr += sizeof(long);

            printf("Read current calibration data from XInput: min_x=%d, max_x=%d and min_y=%d, max_y=%d\n",
                oldcalib_min_x, oldcalib_max_x, oldcalib_min_y, oldcalib_max_y);
        }

        XFree(data);
    } else
        printf("\tFetch failure\n");

}

CalibratorEvdev::~CalibratorEvdev () {
    XCloseDevice(display, dev);
    XCloseDisplay(display);
}

void CalibratorEvdev::finish_data (
    int min_x, int max_x, int min_y, int max_y, int swap_xy, int flip_x, int flip_y)
{
  printf("\nTo make the changes permanent, create add a startup script to your window manager with the following command(s):\n");
  if (swap_xy)
    printf(" xinput set-int-prop \"%s\" \"Evdev Axes Swap\" 8 %d\n", drivername, swap_xy);
  printf(" xinput set-int-prop \"%s\" \"Evdev Axis Calibration\" 32 %d %d %d %d\n", drivername, min_x, max_x, min_y, max_y);


  printf("\nDoing dynamic recalibration:\n");
  // Evdev Axes Swap
  if (swap_xy) {
    printf("\tSwapping X and Y axis...\n");
    // xinput set-int-prop 4 224 8 0
    char str_prop[50];
    sprintf(str_prop, "Evdev Axes Swap");
    char str_swap_xy[20];
    sprintf(str_swap_xy, "%d", swap_xy);

    int argc = 3;
    char* argv[argc];
    //argv[0] = "";
    argv[1] = str_prop;
    argv[2] = str_swap_xy;
    do_set_prop(display, XA_INTEGER, 8, argc, argv);
  }

  // Evdev Axis Inversion
  /* Ignored, X server can handle it by flipping min/max values. */

  // Evdev Axis Calibration
  // xinput set-int-prop 4 223 32 5 500 8 300
  printf("\tSetting new calibration data: %d, %d, %d, %d\n", min_x, max_x, min_y, max_y);
  char str_prop[50];
  sprintf(str_prop, "Evdev Axis Calibration");
  char str_min_x[20];
  sprintf(str_min_x, "%d", min_x);
  char str_max_x[20];
  sprintf(str_max_x, "%d", max_x);
  char str_min_y[20];
  sprintf(str_min_y, "%d", min_y);
  char str_max_y[20];
  sprintf(str_max_y, "%d", max_y);

  int argc = 6;
  char* argv[argc];
  //argv[0] = "";
  argv[1] = str_prop;
  argv[2] = str_min_x;
  argv[3] = str_max_x;
  argv[4] = str_min_y;
  argv[5] = str_max_y;
  do_set_prop(display, XA_INTEGER, 32, argc, argv);

  // close
  XSync(display, False);
}

Bool CalibratorEvdev::check_driver(const char *name) {
    Display     *display;
    XDeviceInfo *info;
    XDevice     *dev;
    int         nprops;
    Atom        *props;
    Bool        found = False;

    display = XOpenDisplay(NULL);
    if (display == NULL) {
	    fprintf(stderr, "Unable to connect to X server\n");
        quit(1);
    }

    info = find_device_info(display, name, False);
    if (!info)
    {
        fprintf(stderr, "unable to find device %s\n", name);
        return NULL;
    }

    dev = XOpenDevice(display, info->id);
    if (!dev)
    {
        fprintf(stderr, "unable to open device '%s'\n", info->name);
        return NULL;
    }

    props = XListDeviceProperties(display, dev, &nprops);
    if (!nprops)
    {
        printf("Device '%s' does not report any properties.\n", info->name);
        return NULL;
    }

    // check if "Evdev Axis Calibration" exists, then its an evdev driver
    while(nprops--)
    {
        if (strcmp(XGetAtomName(display, props[nprops]),
                   "Evdev Axis Calibration") == 0) {
            found = True;
            break;
        }
    }

    XFree(props);
    XCloseDevice(display, dev);
    XCloseDisplay(display);

    return found;
}

Atom CalibratorEvdev::parse_atom(Display *display, const char *name) {
    Bool is_atom = True;
    int i;

    for (i = 0; name[i] != '\0'; i++) {
        if (!isdigit(name[i])) {
            is_atom = False;
            break;
        }
    }

    if (is_atom)
        return atoi(name);
    else
        return XInternAtom(display, name, False);
}

XDeviceInfo* CalibratorEvdev::find_device_info(
Display *display, const char *name, Bool only_extended)
{
    XDeviceInfo	*devices;
    XDeviceInfo *found = NULL;
    int		loop;
    int		num_devices;
    int		len = strlen(name);
    Bool	is_id = True;
    XID		id = (XID)-1;

    for (loop=0; loop<len; loop++) {
        if (!isdigit(name[loop])) {
	        is_id = False;
	        break;
        }
    }

    if (is_id) {
	    id = atoi(name);
    }

    devices = XListInputDevices(display, &num_devices);

    for (loop=0; loop<num_devices; loop++) {
        if ((!only_extended || (devices[loop].use >= IsXExtensionDevice)) &&
	        ((!is_id && strcmp(devices[loop].name, name) == 0) ||
	         (is_id && devices[loop].id == id))) {
            if (found) {
                fprintf(stderr,
	                    "Warning: There are multiple devices named \"%s\".\n"
	                    "To ensure the correct one is selected, please use "
	                    "the device ID instead.\n\n", name);
                return NULL;
            } else {
                found = &devices[loop];
            }
        }
    }

    return found;
}

int CalibratorEvdev::do_set_prop(
Display *display, Atom type, int format, int argc, char **argv)
{
    Atom          prop;
    Atom          old_type;
    char         *name;
    int           i;
    Atom          float_atom;
    int           old_format, nelements = 0;
    unsigned long act_nitems, bytes_after;
    char         *endptr;
    union {
        unsigned char *c;
        short *s;
        long *l;
        Atom *a;
    } data;

    if (argc < 3)
    {
        fprintf(stderr, "Wrong usage of do_set_prop, need at least 3 arguments\n");
        return EXIT_FAILURE;
    }

    name = argv[1];

    prop = parse_atom(display, name);

    if (prop == None) {
        fprintf(stderr, "invalid property %s\n", name);
        return EXIT_FAILURE;
    }

    float_atom = XInternAtom(display, "FLOAT", False);

    nelements = argc - 2;
    if (type == None || format == 0) {
        if (XGetDeviceProperty(display, dev, prop, 0, 0, False, AnyPropertyType,
                               &old_type, &old_format, &act_nitems,
                               &bytes_after, &data.c) != Success) {
            fprintf(stderr, "failed to get property type and format for %s\n",
                    name);
            return EXIT_FAILURE;
        } else {
            if (type == None)
                type = old_type;
            if (format == 0)
                format = old_format;
        }

        XFree(data.c);
    }

    if (type == None) {
        fprintf(stderr, "property %s doesn't exist, you need to specify "
                "its type and format\n", name);
        return EXIT_FAILURE;
    }

    data.c = (unsigned char*)calloc(nelements, sizeof(long));

    for (i = 0; i < nelements; i++)
    {
        if (type == XA_INTEGER) {
            switch (format)
            {
                case 8:
                    data.c[i] = atoi(argv[2 + i]);
                    break;
                case 16:
                    data.s[i] = atoi(argv[2 + i]);
                    break;
                case 32:
                    data.l[i] = atoi(argv[2 + i]);
                    break;
                default:
                    fprintf(stderr, "unexpected size for property %s", name);
                    return EXIT_FAILURE;
            }
        } else if (type == float_atom) {
            if (format != 32) {
                fprintf(stderr, "unexpected format %d for property %s\n",
                        format, name);
                return EXIT_FAILURE;
            }
            *(float *)(data.l + i) = strtod(argv[2 + i], &endptr);
            if (endptr == argv[2 + i]) {
                fprintf(stderr, "argument %s could not be parsed\n", argv[2 + i]);
                return EXIT_FAILURE;
            }
        } else if (type == XA_ATOM) {
            if (format != 32) {
                fprintf(stderr, "unexpected format %d for property %s\n",
                        format, name);
                return EXIT_FAILURE;
            }
            data.a[i] = parse_atom(display, argv[2 + i]);
        } else {
            fprintf(stderr, "unexpected type for property %s\n", name);
            return EXIT_FAILURE;
        }
    }

    XChangeDeviceProperty(display, dev, prop, type, format, PropModeReplace,
                          data.c, nelements);
    free(data.c);
    XCloseDevice(display, dev);
    return EXIT_SUCCESS;
}


/**********************************
 * Class for usbtouchscreen driver,
 * writes output parameters to running kernel and to modprobe.conf
 **********************************/
class CalibratorUsbts: public Calibrator
{
public:
  CalibratorUsbts ();
  ~CalibratorUsbts ();

  virtual void finish_data (int, int, int, int, int, int, int);

protected:
  /* Globals for kernel parameters from startup. We revert to these if the program aborts */
  bool val_transform_xy, val_flip_x, val_flip_y, val_swap_xy;

  /* Helper functions */
  char yesno (const bool value)
  {
    if (value)
      return 'Y';
    else
      return 'N';
  }

  void read_int_parameter (const char *param, int &value)
  {
    char filename [100];
    sprintf (filename, "%s/%s", module_prefix, param);
    FILE *fid = fopen (filename, "r");
    if (fid == NULL)
      {
        fprintf (stderr, "Could not read parameter '%s'\n", param);
        return;
      }

    fscanf (fid, "%d", &value);
    fclose (fid);
  }

  void read_bool_parameter (const char *param, bool &value)
  {
    char filename [100];
    sprintf (filename, "%s/%s", module_prefix, param);
    FILE *fid = fopen (filename, "r");
    if (fid == NULL)
      {
        fprintf (stderr, "Could not read parameter '%s'\n", param);
        return;
      }

    char val [3];
    fgets (val, 2, fid);
    fclose (fid);

    value = (val [0] == yesno (true));
  }

  void write_int_parameter (const char *param, const int value)
  {
    char filename [100];
    sprintf (filename, "%s/%s", module_prefix, param);
    FILE *fid = fopen (filename, "w");
    if (fid == NULL)
      {
        fprintf (stderr, "Could not save parameter '%s'\n", param);
        return;
      }

    fprintf (fid, "%d", value);
    fclose (fid);
  }

  void write_bool_parameter (const char *param, const bool value)
  {
    char filename [100];
    sprintf (filename, "%s/%s", module_prefix, param);
    FILE *fid = fopen (filename, "w");
    if (fid == NULL)
      {
        fprintf (stderr, "Could not save parameter '%s'\n", param);
        return;
      }

    fprintf (fid, "%c", yesno (value));
    fclose (fid);
  }

};

CalibratorUsbts::CalibratorUsbts ()
  : Calibrator ("usbtouchscreen",0,1023,0,1023)
{
  /* Reset the currently running kernel */
  read_bool_parameter (p_transform_xy, val_transform_xy);
  read_bool_parameter (p_flip_x, val_flip_x);
  read_bool_parameter (p_flip_y, val_flip_y);
  read_bool_parameter (p_swap_xy, val_swap_xy);

  write_bool_parameter (p_transform_xy, false);
  write_bool_parameter (p_flip_x, false);
  write_bool_parameter (p_flip_y, false);
  write_bool_parameter (p_swap_xy, false);
}

CalibratorUsbts::~CalibratorUsbts ()
{
  // Dirty exit, so we restore the parameters of the running kernel
  write_bool_parameter (p_transform_xy, val_transform_xy);
  write_bool_parameter (p_flip_x, val_flip_x);
  write_bool_parameter (p_flip_y, val_flip_y);
  write_bool_parameter (p_swap_xy, val_swap_xy);
}

void CalibratorUsbts::finish_data (
    int min_x, int max_x, int min_y, int max_y, int swap_xy, int flip_x, int flip_y)
{
  // not sure this is actually right, this is the old range...
  const int range_x = (oldcalib_max_x - oldcalib_min_x);
  const int range_y = (oldcalib_max_y - oldcalib_min_y);

  /* Send the estimated parameters to the currently running kernel */
  write_int_parameter (p_range_x, range_x);
  write_int_parameter (p_range_y, range_y);
  write_int_parameter (p_min_x, min_x);
  write_int_parameter (p_min_y, min_y);
  write_int_parameter (p_max_x, max_x);
  write_int_parameter (p_max_y, max_y);
  write_bool_parameter (p_transform_xy, true);
  write_bool_parameter (p_flip_x, flip_x);
  write_bool_parameter (p_flip_y, flip_y);
  write_bool_parameter (p_swap_xy, swap_xy);

  /* Write calibration parameters to modprobe_conf_local to keep the for the next boot */
  FILE *fid = fopen (modprobe_conf_local, "r");
  if (fid == NULL)
    {
      fprintf (stderr, "Error: Can't open '%s' for reading. Make sure you have the necesary rights\n",
               modprobe_conf_local);
      // hopefully the destructor of this object will be called to restore params
      quit (1);
    }
  std::string new_contents;
  const int len = 1024; // XXX: we currently don't handle lines that are longer than this
  char line [len];
  const char *opt = "options usbtouchscreen";
  const int opt_len = strlen (opt);
  while (fgets (line, len, fid))
    {
      if (strncmp (line, opt, opt_len) == 0) // This is the line we want to remove
        continue;
      new_contents += line;
    }
  fclose (fid);
  char new_opt [opt_len];
  sprintf (new_opt, "%s %s=%d %s=%d %s=%d %s=%d %s=%d %s=%d %s=%c %s=%c %s=%c %s=%c\n",
           opt, p_range_x, range_x, p_range_y, range_y, p_min_x, min_x, p_min_y, min_y,
           p_max_x, max_x, p_max_y, max_y, p_transform_xy, yesno (true),
           p_flip_x, yesno (flip_x), p_flip_y, yesno (flip_y), p_swap_xy, yesno (swap_xy));
  new_contents += new_opt;

  fid = fopen (modprobe_conf_local, "w");
  if (fid == NULL)
    {
      fprintf (stderr, "Error: Can't open '%s' for writing. Make sure you have the necesary rights\n",
               modprobe_conf_local);
      // hopefully the destructor of this object will be called to restore params
      quit (1);
    }
  fprintf (fid, "%s", new_contents.c_str ());
  fclose (fid);
}



/*****************************************
 * Class for creating the calibration GUI
 *****************************************/
class CalibrationArea : public Gtk::DrawingArea
{
public:
  CalibrationArea (int argc, char** argv);

protected:
  /* Helper functions */
  void redraw ();

  /* Signal handlers */
  bool on_timeout ();
  bool on_expose_event (GdkEventExpose *event);
  bool on_button_press_event (GdkEventButton *event);

  /* Data */
  double X [num_points], Y [num_points];
  int num_clicks, width, height;
  Calibrator* W;
  int time_elapsed;
};

CalibrationArea::CalibrationArea (int argc, char** argv)
  : num_clicks (0), time_elapsed (0)
{
  /* Not sure this is the right place for this, but here we go
   * Get driver name and axis information from XInput */
  {
    int ndevices;
    Display  *display;
    XDeviceInfoPtr list, slist;
    XAnyClassPtr any;
    int xi_opcode, event, error;

    int found = 0;
    const char* drivername = "";
    int min_x = 0, max_x = 0;
    int min_y = 0, max_y = 0;

    display = XOpenDisplay(NULL);

    if (display == NULL) {
	    fprintf(stderr, "Unable to connect to X server\n");
        quit(1);
    }

    if (!XQueryExtension(display, "XInputExtension", &xi_opcode, &event, &error)) {
        printf("X Input extension not available.\n");
        quit(1);
    }

    slist=list=(XDeviceInfoPtr) XListInputDevices (display, &ndevices);
    for (int i=0; i<ndevices; i++, list++)
    {
        if (list->use == IsXKeyboard || list->use == IsXPointer)
            continue;

        any = (XAnyClassPtr) (list->inputclassinfo);
        for (int j=0; j<list->num_classes; j++)
        {

            if (any->c_class == ValuatorClass)
            {
                XValuatorInfoPtr V = (XValuatorInfoPtr) any;
                XAxisInfoPtr ax = (XAxisInfoPtr) V->axes;

                if (V->num_axes >= 2 &&
                    !(ax[0].min_value == -1 && ax[0].max_value == -1) &&
                    !(ax[1].min_value == -1 && ax[1].max_value == -1)) {
                    /* a calibratable device (no mouse etc) */
                    found++;
                    drivername = list->name;
                    min_x = ax[0].min_value;
                    max_x = ax[0].max_value;
                    min_y = ax[1].min_value;
                    max_y = ax[1].max_value;
                }

            }

            /*
             * Increment 'any' to point to the next item in the linked
             * list.  The length is in bytes, so 'any' must be cast to
             * a character pointer before being incremented.
             */
            any = (XAnyClassPtr) ((char *) any + any->length);
        }

    }
    XFreeDeviceList(slist);
    XCloseDisplay(display);

    // override min/maxX/Y from command line ?
    if (argc > 1) {
        for (int i=1; i!=argc; i++) {
            if (strcmp("--precalib", argv[i]) == 0) {
                if (argc > i+1)
                    min_x = atoi(argv[i+1]);
                if (argc > i+2)
                    max_x = atoi(argv[i+2]);
                if (argc > i+3)
                    min_y = atoi(argv[i+3]);
                if (argc > i+4)
                    max_y = atoi(argv[i+4]);
            }
        }
    }

    if (found == 0) {
        fprintf (stderr, "Error: No calibratable devices found.\n");
        quit (1);
    }
    if (found > 1)
        printf ("Warning: multiples calibratable devices found, calibrating last one (%s)\n", drivername);

    /* Different driver, different backend behaviour (case sensitive ?) */
    if (strcmp(drivername, "Usbtouchscreen") == 0)
        W = new CalibratorUsbts();
    else {
        // unable to know device driver from its name alone
        // either its EVDEV or an unknown driver
        // evtouch has: "EVTouch TouchScreen"
        if (CalibratorEvdev::check_driver(drivername))
            W = new CalibratorEvdev(drivername,
                        min_x, max_x, min_y, max_y);
        else
            W = new CalibratorXorgPrint(drivername,
                        min_x, max_x, min_y, max_y);
    }
  }

  /* Listen for mouse events */
  add_events (Gdk::BUTTON_PRESS_MASK);

  /* Compute absolute circle centers */
  const Glib::RefPtr<Gdk::Screen> S = get_screen ();
  width = S->get_width ();
  height = S->get_height ();
  W->set_width (width);
  W->set_height (height);

  const int delta_x = width/num_blocks;
  const int delta_y = height/num_blocks;
  X [UL] = delta_x;             Y [UL] = delta_y;
  X [UR] = width - delta_x - 1; Y [UR] = delta_y;
  X [LL] = delta_x;             Y [LL] = height - delta_y - 1;
  X [LR] = width - delta_x - 1; Y [LR] = height - delta_y - 1;

  /* Setup timer for animation */
  sigc::slot<bool> slot = sigc::mem_fun (*this, &CalibrationArea::on_timeout);
  Glib::signal_timeout().connect (slot, time_step);
}

bool CalibrationArea::on_button_press_event (GdkEventButton *event)
{
  /* Handle click */
  time_elapsed = 0;
  const bool accepted = W->add_click (event->x_root, event->y_root);
  if (accepted)
    num_clicks ++;

  /* Are we done yet? */
  if (num_clicks >= num_points)
    {
      /* Save data to disk */
      W->finish ();

      /* Quit */
      delete W;
      quit (0);
    }

  /* Force a redraw */
  redraw ();

  return true;
}

void CalibrationArea::redraw ()
{
  Glib::RefPtr<Gdk::Window> win = get_window ();
  if (win)
    {
      const Gdk::Rectangle rect (0, 0, width, height);
      win->invalidate_rect (rect, false);
    }
}

bool CalibrationArea::on_expose_event (GdkEventExpose *event)
{
  const double radius = 4;
  Glib::RefPtr<Gdk::Window> window = get_window ();
  if (window)
    {
      Cairo::RefPtr<Cairo::Context> cr = window->create_cairo_context ();
      cr->save ();

      cr->rectangle (event->area.x, event->area.y, event->area.width, event->area.height);
      cr->clip ();

      /* Print the text */
      Cairo::TextExtents extent;
      cr->set_font_size (font_size);
      cr->get_text_extents (help_text, extent);
      cr->move_to ((width-extent.width)/2, 0.3*height);
      cr->show_text (help_text);
      cr->stroke ();

      /* Draw the points */
      for (int i = 0; i < num_clicks; i++)
        {
          cr->arc (X [i], Y [i], radius, 0.0, 2.0 * M_PI);
          cr->set_source_rgb (1.0, 1.0, 1.0);
          cr->fill_preserve ();
          cr->stroke ();
        }

      cr->set_line_width (2);
      cr->arc (X [num_clicks], Y [num_clicks], radius, 0.0, 2.0 * M_PI);
      cr->set_source_rgb (0.8, 0.0, 0.0);
      //cr->fill_preserve ();
      cr->stroke ();

      /* Draw the clock */
      cr->arc (width/2, height/2, clock_radius, 0.0, 2.0 * M_PI);
      cr->set_source_rgb (0.5, 0.5, 0.5);
      cr->fill_preserve ();
      cr->stroke ();

      cr->set_line_width (clock_line_width);
      cr->arc (width/2, height/2, clock_radius - clock_line_width/2, 0.0,
               2.0 * M_PI * (double)time_elapsed/(double)max_time);
      cr->set_source_rgb (0.0, 0.0, 0.0);
      cr->stroke ();

      cr->restore ();
    }

  return true;
}

bool CalibrationArea::on_timeout ()
{
  time_elapsed += time_step;
  if (time_elapsed > max_time) {
    delete W;
    quit (1);
  }

  // Update clock
  Glib::RefPtr<Gdk::Window> win = get_window ();
  if (win)
    {
      const Gdk::Rectangle rect (width/2 - clock_radius - clock_line_width,
                                 height/2 - clock_radius - clock_line_width,
                                 2 * clock_radius + 1 + 2 * clock_line_width,
                                 2 * clock_radius + 1 + 2 * clock_line_width);
      win->invalidate_rect (rect, false);
    }

  return true;
}

static void
usage(char* cmd)
{
    fprintf(stderr, "usage: %s [-h|--help] [--precalib <minx> <maxx> <miny> <maxy>]\n", cmd);
    fprintf(stderr, "\t--precalib: manually provide the current calibration setting (eg the values in xorg.conf)\n");
}

int main(int argc, char** argv)
{
  // Display help ?
  if (argc > 1) {
    for (int i=1; i!=argc; i++) {
        if (strcmp("-h", argv[i]) == 0 ||
             strcmp("--help", argv[i]) == 0) {
            usage(argv[0]);
            return 0;
        }
    }
  }

  Gtk::Main kit(argc, argv);

  Gtk::Window win;
  win.fullscreen ();

  CalibrationArea area(argc, argv);
  win.add (area);
  area.show ();

  Gtk::Main::run (win);

  return 0;
}
