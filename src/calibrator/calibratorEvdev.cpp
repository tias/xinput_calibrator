/*
 * Copyright (c) 2009 Tias Guns
 * Copyright 2007 Peter Hutterer (xinput_ methods from xinput)
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
#include <ctype.h>

#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>
#include <X11/Xatom.h>
//#include <X11/Xutil.h>

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 1
#endif
#ifndef EXIT_FAILURE
#define EXIT_FAILURE 0
#endif

/***************************************
 * Class for dynamic evdev calibration
 * uses xinput "Evdev Axis Calibration"
 ***************************************/
class CalibratorEvdev: public Calibrator
{
private:
    Display     *display;
    XDeviceInfo *info;
    XDevice     *dev;

    int old_swap_xy;
public:
    CalibratorEvdev(const char* const device_name, const XYinfo& axys, const bool verbose, XID device_id=(XID)-1);
    ~CalibratorEvdev();

    virtual bool finish_data(const XYinfo new_axys, int swap_xy);

    bool set_swapxy(const int swap_xy);
    bool set_calibration(const XYinfo new_axys);

    // xinput_ functions (from the xinput project)
    Atom xinput_parse_atom(Display *display, const char* name);
    XDeviceInfo* xinput_find_device_info(Display *display, const char* name, Bool only_extended);
    int xinput_do_set_prop(Display *display, Atom type, int format, int argc, char* argv[]);
};

CalibratorEvdev::CalibratorEvdev(const char* const device_name0, const XYinfo& axys0, const bool verbose0, XID device_id)
  : Calibrator(device_name0, axys0, verbose0), old_swap_xy(0)
{
    // init
    display = XOpenDisplay(NULL);
    if (display == NULL) {
        throw WrongCalibratorException("Evdev: Unable to connect to X server");
    }

    // normaly, we already have the device id
    if (device_id == (XID)-1) {
        info = xinput_find_device_info(display, device_name, False);
        if (!info) {
            XCloseDisplay(display);
            throw WrongCalibratorException("Evdev: Unable to find device");
        }
        device_id = info->id;
    }

    dev = XOpenDevice(display, device_id);
    if (!dev) {
        XCloseDisplay(display);
        throw WrongCalibratorException("Evdev: Unable to open device");
    }

    // check X Input version >= 1.5
    XExtensionVersion *version = XGetExtensionVersion(display, INAME);
    if (version && (version != (XExtensionVersion*) NoSuchExtension)) {
        if (version->major_version < 1 or
            (version->major_version == 1 and version->minor_version < 5)) {
            XFree(version);
            throw WrongCalibratorException("Evdev: your X server is too old, for dynamic recalibration of evdev you need at least XServer 1.6 and X Input 1.5");
        }
        XFree(version);
    }

    // XGetDeviceProperty vars
    Atom            property;
    Atom            act_type;
    int             act_format;
    unsigned long   nitems, bytes_after;
    unsigned char   *data, *ptr;

    // get "Evdev Axis Calibration" property
    property = xinput_parse_atom(display, "Evdev Axis Calibration");
    if (XGetDeviceProperty(display, dev, property, 0, 1000, False,
                           AnyPropertyType, &act_type, &act_format,
                           &nitems, &bytes_after, &data) != Success)
    {
        XCloseDevice(display, dev);
        XCloseDisplay(display);
        throw WrongCalibratorException("Evdev: \"Evdev Axis Calibration\" property missing, not a (valid) evdev device");

    } else {
        if (act_format != 32 || act_type != XA_INTEGER) {
            XCloseDevice(display, dev);
            XCloseDisplay(display);
            throw WrongCalibratorException("Evdev: invalid \"Evdev Axis Calibration\" property format");

        } else if (nitems == 0) {
            if (verbose)
                printf("DEBUG: Evdev Axis Calibration not set, setting to axis valuators to be sure.\n");

            // No axis calibration set, set it to the default one
            // QUIRK: when my machine resumes from a sleep,
            // the calibration property is no longer exported thourgh xinput, but still active
            // not setting the values here would result in a wrong first calibration
            bool ok = set_calibration(old_axys);

            if (verbose) {
                if (ok)
                    printf("DEBUG: Succesfully applied axis calibration.\n");
                else
                    printf("DEBUG: Failed to apply axis calibration.\n");
            }

        } else if (nitems > 0) {
            ptr = data;

            old_axys.x_min = *((long*)ptr);
            ptr += sizeof(long);
            old_axys.x_max = *((long*)ptr);
            ptr += sizeof(long);
            old_axys.y_min = *((long*)ptr);
            ptr += sizeof(long);
            old_axys.y_max = *((long*)ptr);
            ptr += sizeof(long);
        }

        XFree(data);
    }

    // get "Evdev Axes Swap" property
    property = xinput_parse_atom(display, "Evdev Axes Swap");
    if (XGetDeviceProperty(display, dev, property, 0, 1000, False,
                           AnyPropertyType, &act_type, &act_format,
                           &nitems, &bytes_after, &data) == Success)
    {
        if (act_format == 8 && act_type == XA_INTEGER && nitems == 1) {
            old_swap_xy = *((char*)data);

            if (verbose)
                printf("DEBUG: Read axes swap value of %i.\n", old_swap_xy);
        }
    }
    // TODO? evdev < 2.3.2 with swap_xy had a bug which calibrated before swapping (eg X calib on Y axis)
    // see http://cgit.freedesktop.org/xorg/driver/xf86-input-evdev/commit/?h=evdev-2.3-branch&id=3772676fd65065b43a94234127537ab5030b09f8

    printf("Calibrating EVDEV driver for \"%s\" id=%i\n", device_name, (int)device_id);
    printf("\tcurrent calibration values (from XInput): min_x=%d, max_x=%d and min_y=%d, max_y=%d\n",
                old_axys.x_min, old_axys.x_max, old_axys.y_min, old_axys.y_max);
}

CalibratorEvdev::~CalibratorEvdev () {
    XCloseDevice(display, dev);
    XCloseDisplay(display);
}

bool CalibratorEvdev::finish_data(const XYinfo new_axys, int swap_xy)
{
    bool success = true;

    // swap x and y: check if we need to swap with: if (swap_xy)
    // value to swap too is inverse of previous swap value: in new_swap_xy
    int new_swap_xy = 1 - old_swap_xy;

    printf("\nDoing dynamic recalibration:\n");
    // Evdev Axes Swap
    if (swap_xy) {
        printf("\tSwapping X and Y axis...\n");
        bool ok = set_swapxy(new_swap_xy);
        success &= ok;

        if (verbose) {
            if (ok)
                printf("DEBUG: Succesfully swapped X and Y axis.\n");
            else
                printf("DEBUG: Failed to swap X and Y axis.\n");
        }
    }

    // Evdev Axis Calibration
    printf("\tSetting new calibration data: %d, %d, %d, %d\n", new_axys.x_min, new_axys.x_max, new_axys.y_min, new_axys.y_max);
    bool ok = set_calibration(new_axys);
    success &= ok;

    if (verbose) {
        if (ok)
            printf("DEBUG: Succesfully applied axis calibration.\n");
        else
            printf("DEBUG: Failed to apply axis calibration.\n");
    }
    // close
    XSync(display, False);


    // on stdout: ways to make calibration permanent
    printf("\n\n== Saving the calibration ==\n");
    // create startup script
    printf("If you have the 'xinput' tool installed, a simple way is to create a script that starts with your X session, containing the following command(s):\n");
    printf("    xinput set-int-prop \"%s\" \"Evdev Axis Calibration\" 32 %d %d %d %d\n", device_name, new_axys.x_min, new_axys.x_max, new_axys.y_min, new_axys.y_max);
    if (swap_xy)
        printf("    xinput set-int-prop \"%s\" \"Evdev Axes Swap\" 8 %d\n", device_name, new_swap_xy);
    printf("See scripts/xinput_calibrator_pointercal.sh for an example used on mobile devices\n");


    // TODO detect EVDEV version at runtime ?
    // deliberately not mention HAL way, by the time users run evdev 2.3.0, there will be no more HAL (on linux)
    printf("\nIf you have evdev version 2.3.0 or higher, there are 2 more ways: the tranditional way (xorg.conf) and the new way (udev rule):\n");

    // Xorg.conf output
    printf("xorg.conf: edit /etc/X11/xorg.conf and add in the 'Section \"InputDevice\"' of your device:\n");
    printf("    Option\t\"Calibration\"\t\t\"%d %d %d %d\"\n",
                new_axys.x_min, new_axys.x_max, new_axys.y_min, new_axys.y_max);
    if (swap_xy)
        printf("    Option\t\"SwapAxes\"\t\"%d\" # unless it was already set to 1\n", new_swap_xy);

    // udev rule
    printf("udev rule: create the file '/etc/udev/rules.d/99_touchscreen.rules' with:\n\
    ACTION!=\"add|change\", GOTO=\"xorg_touchscreen_end\"\n\
    KERNEL!=\"event*\", GOTO=\"xorg_touchscreen_end\"\n\
    ATTRS{product}!=\"%s\", GOTO=\"xorg_touchscreen_end\"\n\
    ENV{x11_options.calibration}=\"%d %d %d %d\"\n",
         device_name, new_axys.x_min, new_axys.x_max, new_axys.y_min, new_axys.y_max);
    if (swap_xy)
        printf("    ENV{x11_options.swapxy}=\"%d\"\n", new_swap_xy);
    printf("    LABEL=\"xorg_touchscreen_end\"\n");


    return success;
}

bool CalibratorEvdev::set_swapxy(const int swap_xy)
{
    // xinput set-int-prop "divername" "Evdev Axes Swap" 8 0
    char* arr_cmd[3];
    //arr_cmd[0] = "";
    char str_prop[50];
    sprintf(str_prop, "Evdev Axes Swap");
    arr_cmd[1] = str_prop;
    char str_swap_xy[20];
    sprintf(str_swap_xy, "%d", swap_xy);
    arr_cmd[2] = str_swap_xy;

    int ret = xinput_do_set_prop(display, XA_INTEGER, 8, 3, arr_cmd);
    return (ret == EXIT_SUCCESS);
}

bool CalibratorEvdev::set_calibration(const XYinfo new_axys)
{
    // xinput set-int-prop 4 223 32 5 500 8 300
    char* arr_cmd[6];
    //arr_cmd[0] = "";
    char str_prop[50];
    sprintf(str_prop, "Evdev Axis Calibration");
    arr_cmd[1] = str_prop;
    char str_min_x[20];
    sprintf(str_min_x, "%d", new_axys.x_min);
    arr_cmd[2] = str_min_x;
    char str_max_x[20];
    sprintf(str_max_x, "%d", new_axys.x_max);
    arr_cmd[3] = str_max_x;
    char str_min_y[20];
    sprintf(str_min_y, "%d", new_axys.y_min);
    arr_cmd[4] = str_min_y;
    char str_max_y[20];
    sprintf(str_max_y, "%d", new_axys.y_max);
    arr_cmd[5] = str_max_y;

    int ret = xinput_do_set_prop(display, XA_INTEGER, 32, 6, arr_cmd);
    return (ret == EXIT_SUCCESS);
}

Atom CalibratorEvdev::xinput_parse_atom(Display *display, const char *name) {
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

XDeviceInfo* CalibratorEvdev::xinput_find_device_info(
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

int CalibratorEvdev::xinput_do_set_prop(
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
        fprintf(stderr, "Wrong usage of xinput_do_set_prop, need at least 3 arguments\n");
        return EXIT_FAILURE;
    }

    name = argv[1];

    prop = xinput_parse_atom(display, name);

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
            data.a[i] = xinput_parse_atom(display, argv[2 + i]);
        } else {
            fprintf(stderr, "unexpected type for property %s\n", name);
            return EXIT_FAILURE;
        }
    }

    XChangeDeviceProperty(display, dev, prop, type, format, PropModeReplace,
                          data.c, nelements);
    free(data.c);
    return EXIT_SUCCESS;
}
