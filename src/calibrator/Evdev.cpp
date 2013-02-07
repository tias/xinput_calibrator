/*
 * Copyright (c) 2009 Tias Guns
 * Copyright 2007 Peter Hutterer (xinput_ methods from xinput)
 * Copyright (c) 2011 Antoine Hue (invertX/Y)
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

#include "calibrator/Evdev.hpp"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <ctype.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 1
#endif
#ifndef EXIT_FAILURE
#define EXIT_FAILURE 0
#endif

// Constructor
CalibratorEvdev::CalibratorEvdev(const char* const device_name0,
                                 const XYinfo& axys0,
                                 XID device_id,
                                 const int thr_misclick,
                                 const int thr_doubleclick,
                                 const OutputType output_type,
                                 const char* geometry,
                                 const bool use_timeout,
                                 const char* output_filename)
  : Calibrator(device_name0, axys0, thr_misclick, thr_doubleclick, output_type, geometry, use_timeout, output_filename)
{
    // init
    display = XOpenDisplay(NULL);
    if (display == NULL) {
        throw WrongCalibratorException("Evdev: Unable to connect to X server");
    }

    // normaly, we already have the device id
    if (device_id == (XID)-1) {
        devInfo = xinput_find_device_info(display, device_name, False);
        if (!devInfo) {
            XCloseDisplay(display);
            throw WrongCalibratorException("Evdev: Unable to find device");
        }
        device_id = devInfo->id;
    }

    dev = XOpenDevice(display, device_id);
    if (!dev) {
        XCloseDisplay(display);
        throw WrongCalibratorException("Evdev: Unable to open device");
    }

#ifndef HAVE_XI_PROP
    throw WrongCalibratorException("Evdev: you need at least libXi 1.2 and inputproto 1.5 for dynamic recalibration of evdev.");
#else

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
            // the calibration property is no longer exported through xinput, but still active
            // not setting the values here would result in a wrong first calibration
            (void) set_calibration(old_axys);

        } else if (nitems > 0) {
            ptr = data;

            old_axys.x.min = *((long*)ptr);
            ptr += sizeof(long);
            old_axys.x.max = *((long*)ptr);
            ptr += sizeof(long);
            old_axys.y.min = *((long*)ptr);
            ptr += sizeof(long);
            old_axys.y.max = *((long*)ptr);
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
            old_axys.swap_xy = *((char*)data);

            if (verbose)
                printf("DEBUG: Read axes swap value of %i.\n", old_axys.swap_xy);
        }
    }

    // get "Evdev Axes Inversion" property
    property = xinput_parse_atom(display, "Evdev Axis Inversion");
    if (XGetDeviceProperty(display, dev, property, 0, 1000, False,
                AnyPropertyType, &act_type, &act_format,
                &nitems, &bytes_after, &data) == Success) {
        if (act_format == 8 && act_type == XA_INTEGER && nitems == 2) {
            old_axys.x.invert = *((char*)data++);
            old_axys.y.invert = *((char*)data);

            if (verbose)
                printf("DEBUG: Read InvertX=%i, InvertY=%i.\n", old_axys.x.invert, old_axys.y.invert);
        }
    }

    printf("Calibrating EVDEV driver for \"%s\" id=%i\n", device_name, (int)device_id);
    printf("\tcurrent calibration values (from XInput): min_x=%d, max_x=%d and min_y=%d, max_y=%d\n",
                old_axys.x.min, old_axys.x.max, old_axys.y.min, old_axys.y.max);
#endif // HAVE_XI_PROP

}
// protected pass-through constructor for subclasses
CalibratorEvdev::CalibratorEvdev(const char* const device_name0,
                                 const XYinfo& axys0,
                                 const int thr_misclick,
                                 const int thr_doubleclick,
                                 const OutputType output_type,
                                 const char* geometry,
                                 const bool use_timeout,
                                 const char* output_filename)
  : Calibrator(device_name0, axys0, thr_misclick, thr_doubleclick, output_type, geometry, output_filename) { }

// Destructor
CalibratorEvdev::~CalibratorEvdev () {
    XCloseDevice(display, dev);
    XCloseDisplay(display);
}

// From Calibrator but with evdev specific invertion option
// KEEP IN SYNC with Calibrator::finish() !!
bool CalibratorEvdev::finish(int width, int height)
{
    if (get_numclicks() != NUM_POINTS) {
        return false;
    }

    // new axis origin and scaling
    // based on old_axys: inversion/swapping is relative to the old axis
    XYinfo new_axis(old_axys);


    // calculate average of clicks
    float x_min = (clicked.x[UL] + clicked.x[LL])/2.0;
    float x_max = (clicked.x[UR] + clicked.x[LR])/2.0;
    float y_min = (clicked.y[UL] + clicked.y[UR])/2.0;
    float y_max = (clicked.y[LL] + clicked.y[LR])/2.0;


    // When evdev detects an invert_X/Y option,
    // it performs the following *crazy* code just before returning
    // val = (pEvdev->absinfo[i].maximum - val + pEvdev->absinfo[i].minimum);
    // undo this crazy step before doing the regular calibration routine
    if (old_axys.x.invert) {
        x_min = width - x_min;
        x_max = width - x_max;
        // avoid invert_x property from here on,
        // the calibration code can handle this dynamically!
        new_axis.x.invert = false;
    }
    if (old_axys.y.invert) {
        y_min = height - y_min;
        y_max = height - y_max;
        // avoid invert_y property from here on,
        // the calibration code can handle this dynamically!
        new_axis.y.invert = false;
    }
    // end of evdev inversion crazyness


    // Should x and y be swapped?
    if (abs(clicked.x[UL] - clicked.x[UR]) < abs(clicked.y[UL] - clicked.y[UR])) {
        new_axis.swap_xy = !new_axis.swap_xy;
        std::swap(x_min, y_min);
        std::swap(x_max, y_max);
    }

    // the screen was divided in num_blocks blocks, and the touch points were at
    // one block away from the true edges of the screen.
    const float block_x = width/(float)num_blocks;
    const float block_y = height/(float)num_blocks;
    // rescale these blocks from the range of the drawn touchpoints to the range of the 
    // actually clicked coordinates, and substract/add from the clicked coordinates
    // to obtain the coordinates corresponding to the edges of the screen.
    float scale_x = (x_max - x_min)/(width - 2*block_x);
    x_min -= block_x * scale_x;
    x_max += block_x * scale_x;
    float scale_y = (y_max - y_min)/(height - 2*block_y);
    y_min -= block_y * scale_y;
    y_max += block_y * scale_y;
    
    // now, undo the transformations done by the X server, to obtain the true 'raw' value in X.
    // The raw value was scaled from old_axis to the device min/max, and from the device min/max
    // to the screen min/max
    // hence, the reverse transformation is from screen to old_axis
    x_min = scaleAxis(x_min, old_axys.x.max, old_axys.x.min, width, 0);
    x_max = scaleAxis(x_max, old_axys.x.max, old_axys.x.min, width, 0);
    y_min = scaleAxis(y_min, old_axys.y.max, old_axys.y.min, height, 0);
    y_max = scaleAxis(y_max, old_axys.y.max, old_axys.y.min, height, 0);


    // round and put in new_axis struct
    new_axis.x.min = round(x_min); new_axis.x.max = round(x_max);
    new_axis.y.min = round(y_min); new_axis.y.max = round(y_max);

    // finish the data, driver/calibrator specific
    return finish_data(new_axis);
}

// Activate calibrated data and output it
bool CalibratorEvdev::finish_data(const XYinfo new_axys)
{
    bool success = true;

    printf("\nDoing dynamic recalibration:\n");
    // Evdev Axes Swap
    if (old_axys.swap_xy != new_axys.swap_xy) {
        success &= set_swapxy(new_axys.swap_xy);
    }

   // Evdev Axis Inversion
   if (old_axys.x.invert != new_axys.x.invert ||
       old_axys.y.invert != new_axys.y.invert) {
        success &= set_invert_xy(new_axys.x.invert, new_axys.y.invert);
    }

    // Evdev Axis Calibration
    success &= set_calibration(new_axys);

    // close
    XSync(display, False);

    printf("\t--> Making the calibration permanent <--\n");
    switch (output_type) {
        case OUTYPE_AUTO:
            // xorg.conf.d or alternatively xinput commands
            if (has_xorgconfd_support()) {
                success &= output_xorgconfd(new_axys);
            } else {
                success &= output_xinput(new_axys);
            }
            break;
        case OUTYPE_XORGCONFD:
            success &= output_xorgconfd(new_axys);
            break;
        case OUTYPE_HAL:
            success &= output_hal(new_axys);
            break;
        case OUTYPE_XINPUT:
            success &= output_xinput(new_axys);
            break;
        default:
            fprintf(stderr, "ERROR: Evdev Calibrator does not support the supplied --output-type\n");
            success = false;
    }

    return success;
}

bool CalibratorEvdev::set_swapxy(const int swap_xy)
{
    printf("\tSwapping X and Y axis...\n");

    // xinput set-int-prop "divername" "Evdev Axes Swap" 8 0
    int arr_cmd[1];
    arr_cmd[0] = swap_xy;

    bool ret = xinput_do_set_int_prop("Evdev Axes Swap", display, 8, 1, arr_cmd);

    if (verbose) {
        if (ret == true)
            printf("DEBUG: Successfully set swapped X and Y axes = %d.\n", swap_xy);
        else
            printf("DEBUG: Failed to set swap X and Y axes.\n");
    }

    return ret;
}

bool CalibratorEvdev::set_invert_xy(const int invert_x, const int invert_y)
{
    printf("\tInverting X and/or Y axis...\n");

    // xinput set-int-prop "divername" "Evdev Axis Inversion" 8 0 0
    int arr_cmd[2];
    arr_cmd[0] = invert_x;
    arr_cmd[1] = invert_y;

    int ret = xinput_do_set_int_prop("Evdev Axis Inversion", display, 8, 2, arr_cmd);

    if (verbose) {
        if (ret == true)
            printf("DEBUG: Successfully set invert axis X=%d, Y=%d.\n", invert_x, invert_y);
        else
            printf("DEBUG: Failed to set axis inversion.\n");
    }

    return ret;
}

bool CalibratorEvdev::set_calibration(const XYinfo new_axys)
{
    printf("\tSetting calibration data: %d, %d, %d, %d\n", new_axys.x.min, new_axys.x.max, new_axys.y.min, new_axys.y.max);

    // xinput set-int-prop 4 223 32 5 500 8 300
    int arr_cmd[4];
    arr_cmd[0] = new_axys.x.min;
    arr_cmd[1] = new_axys.x.max;
    arr_cmd[2] = new_axys.y.min;
    arr_cmd[3] = new_axys.y.max;

    int ret = xinput_do_set_int_prop("Evdev Axis Calibration", display, 32, 4, arr_cmd);

    if (verbose) {
        if (ret == true)
            printf("DEBUG: Successfully applied axis calibration.\n");
        else
            printf("DEBUG: Failed to apply axis calibration.\n");
    }

    return ret;
}

Atom CalibratorEvdev::xinput_parse_atom(Display *display, const char *name)
{
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

// Set Integer property on  X
bool CalibratorEvdev::xinput_do_set_int_prop( const char * name,
                                         Display *display,
                                         int format,
                                         int argc,
                                         const int *argv )
{
#ifndef HAVE_XI_PROP
    return false;
#else

    Atom          prop;
    Atom          old_type;
    int           i;
    int           old_format;
    unsigned long act_nitems, bytes_after;

    union {
        unsigned char *c;
        short *s;
        long *l;
        Atom *a;
    } data;

    if (argc < 1)
    {
        fprintf(stderr, "Wrong usage of xinput_do_set_prop, need at least 1 arguments\n");
        return false;
    }

    prop = xinput_parse_atom(display, name);

    if (prop == None) {
        fprintf(stderr, "invalid property %s\n", name);
        return false;
    }

    if ( format == 0) {
        if (XGetDeviceProperty(display, dev, prop, 0, 0, False, AnyPropertyType,
                               &old_type, &old_format, &act_nitems,
                               &bytes_after, &data.c) != Success) {
            fprintf(stderr, "failed to get property type and format for %s\n",
                    name);
            return false;
        } else {
            format = old_format;
        }

        XFree(data.c);
    }

    data.c = (unsigned char*)calloc(argc, sizeof(long));

    for (i = 0; i < argc; i++) {
      switch (format) {
        case 8:
            data.c[i] = argv[i];
        case 16:
            data.s[i] = argv[i];
            break;
        case 32:
            data.l[i] = argv[i];
            break;

        default:
            fprintf(stderr, "unexpected size for property %s\n", name);
            return false;
      }
    }

    XChangeDeviceProperty(display, dev, prop, XA_INTEGER, format, PropModeReplace,
                      data.c, argc);
    free(data.c);
    return true;
#endif // HAVE_XI_PROP

}

bool CalibratorEvdev::output_xorgconfd(const XYinfo new_axys)
{
    const char* sysfs_name = get_sysfs_name();
    bool not_sysfs_name = (sysfs_name == NULL);
    if (not_sysfs_name)
        sysfs_name = "!!Name_Of_TouchScreen!!";

    if(output_filename == NULL || not_sysfs_name)
        printf("  copy the snippet below into '/etc/X11/xorg.conf.d/99-calibration.conf' (/usr/share/X11/xorg.conf.d/ in some distro's)\n");
    else
        printf("  writing xorg.conf calibration data to '%s'\n", output_filename);

    // xorg.conf.d snippet
    char line[MAX_LINE_LEN];
    std::string outstr;

    outstr += "Section \"InputClass\"\n";
    outstr += "	Identifier	\"calibration\"\n";
    sprintf(line, "	MatchProduct	\"%s\"\n", sysfs_name);
    outstr += line;
    sprintf(line, "	Option	\"Calibration\"	\"%d %d %d %d\"\n",
                new_axys.x.min, new_axys.x.max, new_axys.y.min, new_axys.y.max);
    outstr += line;
    sprintf(line, "	Option	\"SwapAxes\"	\"%d\"\n", new_axys.swap_xy);
    outstr += line;
    outstr += "EndSection\n";

    // console out
    printf("%s", outstr.c_str());
    if (not_sysfs_name)
        printf("\nChange '%s' to your device's name in the snippet above.\n", sysfs_name);
    // file out
    else if(output_filename != NULL) {
        FILE* fid = fopen(output_filename, "w");
        if (fid == NULL) {
            fprintf(stderr, "Error: Can't open '%s' for writing. Make sure you have the necessary rights\n", output_filename);
            fprintf(stderr, "New calibration data NOT saved\n");
            return false;
        }
        fprintf(fid, "%s", outstr.c_str());
        fclose(fid);
    }

    return true;
}

bool CalibratorEvdev::output_hal(const XYinfo new_axys)
{
    const char* sysfs_name = get_sysfs_name();
    bool not_sysfs_name = (sysfs_name == NULL);
    if (not_sysfs_name)
        sysfs_name = "!!Name_Of_TouchScreen!!";

    if(output_filename == NULL || not_sysfs_name)
        printf("  copy the policy below into '/etc/hal/fdi/policy/touchscreen.fdi'\n");
    else
        printf("  writing HAL calibration data to '%s'\n", output_filename);

    // HAL policy output
    char line[MAX_LINE_LEN];
    std::string outstr;

    sprintf(line, "<match key=\"info.product\" contains=\"%s\">\n", sysfs_name);
    outstr += line;
    sprintf(line, "  <merge key=\"input.x11_options.calibration\" type=\"string\">%d %d %d %d</merge>\n",
        new_axys.x.min, new_axys.x.max, new_axys.y.min, new_axys.y.max);
    outstr += line;
    sprintf(line, "  <merge key=\"input.x11_options.swapaxes\" type=\"string\">%d</merge>\n",
        new_axys.swap_xy);
    outstr += "</match>\n";
    // console out
    printf("%s", outstr.c_str());
    if (not_sysfs_name)
        printf("\nChange '%s' to your device's name in the config above.\n", sysfs_name);
    // file out
    else if(output_filename != NULL) {
        FILE* fid = fopen(output_filename, "w");
        if (fid == NULL) {
            fprintf(stderr, "Error: Can't open '%s' for writing. Make sure you have the necessary rights\n", output_filename);
            fprintf(stderr, "New calibration data NOT saved\n");
            return false;
        }
        fprintf(fid, "%s", outstr.c_str());
        fclose(fid);
    }

    return true;
}

bool CalibratorEvdev::output_xinput(const XYinfo new_axys)
{
    if(output_filename == NULL)
        printf("  Install the 'xinput' tool and copy the command(s) below in a script that starts with your X session\n");
    else
        printf("  writing calibration script to '%s'\n", output_filename);

    // create startup script
    char line[MAX_LINE_LEN];
    std::string outstr;

    sprintf(line, "    xinput set-int-prop \"%s\" \"Evdev Axis Calibration\" 32 %d %d %d %d\n", device_name, new_axys.x.min, new_axys.x.max, new_axys.y.min, new_axys.y.max);
    outstr += line;
    sprintf(line, "    xinput set-int-prop \"%s\" \"Evdev Axes Swap\" 8 %d\n", device_name, new_axys.swap_xy);
    outstr += line;

    // console out
    printf("%s", outstr.c_str());
    // file out
    if(output_filename != NULL) {
		FILE* fid = fopen(output_filename, "w");
		if (fid == NULL) {
			fprintf(stderr, "Error: Can't open '%s' for writing. Make sure you have the necessary rights\n", output_filename);
			fprintf(stderr, "New calibration data NOT saved\n");
			return false;
		}
		fprintf(fid, "%s", outstr.c_str());
		fclose(fid);
    }

    return true;
}
