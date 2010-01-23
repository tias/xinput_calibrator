/*
 * Copyright (c) 2009 Tias Guns
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
public:
    CalibratorEvdev(const char* const drivername, const XYinfo& axys);
    ~CalibratorEvdev();

    virtual bool finish_data(const XYinfo new_axys, int swap_xy);

    static Bool check_driver(const char* const name);

    // xinput functions (from the xinput source)
    static Atom parse_atom(Display *display, const char* name);
    static XDeviceInfo* find_device_info(Display *display, const char* name, Bool only_extended);
    int do_set_prop(Display *display, Atom type, int format, int argc, char* argv[]);
};

CalibratorEvdev::CalibratorEvdev(const char* const drivername0, const XYinfo& axys0)
  : Calibrator(drivername0, axys0)
{
    printf("Calibrating EVDEV driver for \"%s\"\n", drivername);

    // init
    display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "Unable to connect to X server\n");
        return;
    }

    info = find_device_info(display, drivername, False);
    if (!info) {
        fprintf(stderr, "unable to find device %s\n", drivername);
        return;
    }

    dev = XOpenDevice(display, info->id);
    if (!dev) {
        fprintf(stderr, "unable to open device '%s'\n", info->name);
        return;
    }

    // get "Evdev Axis Calibration" property
    Atom            property = parse_atom(display, "Evdev Axis Calibration");
    Atom            act_type;
    int             act_format;
    unsigned long   nitems, bytes_after;
    unsigned char   *data, *ptr;
    if (XGetDeviceProperty(display, dev, property, 0, 1000, False,
                           AnyPropertyType, &act_type, &act_format,
                           &nitems, &bytes_after, &data) == Success)
    {
        if (act_format != 32 || act_type != XA_INTEGER) {
            fprintf(stderr, "Error: unexpected format or type from \"Evdev Axis Calibration\" property.\n");
        }

        if (nitems != 0) {
            ptr = data;

            old_axys.x_min = *((long*)ptr);
            ptr += sizeof(long);
            old_axys.x_max = *((long*)ptr);
            ptr += sizeof(long);
            old_axys.y_min = *((long*)ptr);
            ptr += sizeof(long);
            old_axys.y_max = *((long*)ptr);
            ptr += sizeof(long);

            printf("Read current calibration data from XInput: min_x=%d, max_x=%d and min_y=%d, max_y=%d\n",
                old_axys.x_min, old_axys.x_max, old_axys.y_min, old_axys.y_max);
        }

        XFree(data);
    } else
        printf("\tFetch failure for 'Evdev Axis Calibration', continuing nonetheless\n");
}

CalibratorEvdev::~CalibratorEvdev () {
    XCloseDevice(display, dev);
    XCloseDisplay(display);
}

bool CalibratorEvdev::finish_data(const XYinfo new_axys, int swap_xy)
{
    printf("\nTo make the settings permanent, create add a startup script for your window manager with the following command(s):\n");
    if (swap_xy)
        printf(" xinput set-int-prop \"%s\" \"Evdev Axes Swap\" 8 %d\n", drivername, swap_xy);
    printf(" xinput set-int-prop \"%s\" \"Evdev Axis Calibration\" 32 %d %d %d %d\n", drivername, new_axys.x_min, new_axys.x_max, new_axys.y_min, new_axys.y_max);

    bool success = true;

    printf("\nDoing dynamic recalibration:\n");
    // Evdev Axes Swap
    if (swap_xy) {
        printf("\tSwapping X and Y axis...\n");
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
        success &= (ret == EXIT_SUCCESS);
    }

    // Evdev Axis Calibration
    printf("\tSetting new calibration data: %d, %d, %d, %d\n", new_axys.x_min, new_axys.x_max, new_axys.y_min, new_axys.y_max);
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
    success &= (ret == EXIT_SUCCESS);

    // close
    XSync(display, False);

    return success;
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
        //fprintf(stderr, "Unable to connect to X server");
        return false;
    }

    info = find_device_info(display, name, False);
    if (!info)
    {
        XCloseDisplay(display);
        //fprintf(stderr, "unable to find device %s\n", name);
        return false;
    }

    dev = XOpenDevice(display, info->id);
    if (!dev)
    {
        XCloseDisplay(display);
        //fprintf(stderr, "unable to open device '%s'\n", info->name);
        return false;
    }

    props = XListDeviceProperties(display, dev, &nprops);
    if (!nprops)
    {
        XCloseDevice(display, dev);
        XCloseDisplay(display);
        //printf("Device '%s' does not report any properties.\n", info->name);
        return false;
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
    return EXIT_SUCCESS;
}
