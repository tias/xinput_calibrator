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

#include "calibrator/Libinput.hpp"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <ctype.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <cassert>


#define LIBINPUTCALIBRATIONMATRIXPRO "libinput Calibration Matrix"

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 1
#endif
#ifndef EXIT_FAILURE
#define EXIT_FAILURE 0
#endif

void CalibratorLibinput::setIdentity(float coeff[9]) {
    static const float id[] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
    memcpy(coeff, id, sizeof(float) * 9);
}

void CalibratorLibinput::getMatrix(const char *name, float coeff[9]) {

    Atom float_atom = XInternAtom(display, "FLOAT", false);

    // get "Evdev Axis Calibration" property
    Atom property = xinput_parse_atom(name);

    Atom            act_type;
    int             act_format;
    unsigned long   nitems, bytes_after;
    unsigned char   *data;
    if (XGetDeviceProperty(display, dev, property, 0, 1000, False,
                           AnyPropertyType, &act_type, &act_format,
                           &nitems, &bytes_after, &data) != Success) {
        throw WrongCalibratorException("Libinput: \"libinput Calibration Matrix\" property missing, not a (valid) libinput device");
    }

    if (act_type != float_atom || act_format == 32) {
        XFree(data);
        throw WrongCalibratorException("Libinput: \"libinput Calibration Matrix\" property format");

    }

    int size = sizeof(long);

    setIdentity(coeff);
    for (unsigned int i = 0 ; i < nitems ; i++) {
        coeff[i] = *((float *)data);
        data += size;
    }

    XFree(data);
}

void CalibratorLibinput::setMatrix(const char *name, const float coeff[9]) {
    Atom          prop;
    Atom          old_type;
    int           old_format;
    unsigned long act_nitems, bytes_after;
    float         *ptr;



    prop = xinput_parse_atom(name);
    Atom float_atom = XInternAtom(display, "FLOAT", false);

    if (prop == None) {
        fprintf(stderr, "invalid property '%s'\n", name);
        throw WrongCalibratorException("Libinput: \"libinput Calibration Matrix\" property missing, not a (valid) libinput device");
    }

    if (XGetDeviceProperty(display, dev, prop, 0, 0, False,
                           AnyPropertyType,
                           &old_type, &old_format, &act_nitems,
                           &bytes_after, (unsigned char **)&ptr) != Success)
        throw WrongCalibratorException("Libinput: \"libinput Calibration Matrix\" property missing, not a (valid) libinput device");

    XFree(ptr);
    if (old_type != float_atom || old_format != 32)
        throw WrongCalibratorException("Libinput: \"libinput Calibration Matrix\" property format");


    int nelements = 9;
    ptr = (float*)calloc(nelements, sizeof(long));
    assert(ptr);
    for (int i = 0; i < nelements; i++)
         ptr[i] = coeff[i];

    XChangeDeviceProperty(display, dev, prop, old_type, old_format,
                            PropModeReplace, (unsigned char *)ptr, nelements);
    free(ptr);

}

// Constructor
CalibratorLibinput::CalibratorLibinput(const char* const device_name0,
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
    display = NULL;
    dev = NULL;

    // init
    display = XOpenDisplay(NULL);
    if (display == NULL) {
        throw WrongCalibratorException("Libinput: Unable to connect to X server");
    }

    // normaly, we already have the device id
    if (device_id == (XID)-1) {
        devInfo = xinput_find_device_info(display, device_name, False);
        if (!devInfo) {
            XCloseDisplay(display);
            throw WrongCalibratorException("Libinput: Unable to find device");
        }
        device_id = devInfo->id;
    }

    dev = XOpenDevice(display, device_id);
    if (!dev) {
        XCloseDisplay(display);
        throw WrongCalibratorException("Libinput: Unable to open device");
    }

#ifndef HAVE_XI_PROP
    throw WrongCalibratorException("Evdev: you need at least libXi 1.2 and inputproto 1.5 for dynamic recalibration of evdev.");
#else

    getMatrix(LIBINPUTCALIBRATIONMATRIXPRO, old_coeff);
    reset_data = true;

    /* FIXME, allow to override initial coeff */
    float coeff[9];
    setIdentity(coeff);
    setMatrix(LIBINPUTCALIBRATIONMATRIXPRO, coeff);

    getMatrix(LIBINPUTCALIBRATIONMATRIXPRO, coeff);

    printf("Calibrating Libinput driver for \"%s\" id=%i\n", device_name, (int)device_id);
    printf("\tcurrent calibration values (from XInput): {");
    for (int i = 0 ; i < 9 ; i++) {
        printf("%f", coeff[i]);
        if (i < 8 )
            printf(", ");
    }
    printf("}\n");

#endif // HAVE_XI_PROP

}
// protected pass-through constructor for subclasses
CalibratorLibinput::CalibratorLibinput(const char* const device_name0,
                                 const XYinfo& axys0,
                                 const int thr_misclick,
                                 const int thr_doubleclick,
                                 const OutputType output_type,
                                 const char* geometry,
                                 const bool use_timeout,
                                 const char* output_filename)
  : Calibrator(device_name0, axys0, thr_misclick, thr_doubleclick, output_type, geometry, output_filename) { }

// Destructor
CalibratorLibinput::~CalibratorLibinput () {
    if (dev && display) {
        if (reset_data) {
            setMatrix(LIBINPUTCALIBRATIONMATRIXPRO, old_coeff);
        }
        XCloseDevice(display, dev);
    }
    if (display)
        XCloseDisplay(display);
}

// From Calibrator but with evdev specific invertion option
// KEEP IN SYNC with Calibrator::finish() !!
bool CalibratorLibinput::finish(int width, int height)
{
    if (get_numclicks() != NUM_POINTS) {
        return false;
    }

    // new axis origin and scaling
    // based on old_axys: inversion/swapping is relative to the old axis
    //XYinfo new_axis(old_axys);
    float coeff[9];
    setIdentity(coeff);
/*

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

    // finish the data, driver/calibrator specific*/
    return finish_data(coeff);
}

bool CalibratorLibinput::finish_data(const XYinfo &new_axys) { assert(0); return false;}

// Activate calibrated data and output it
bool CalibratorLibinput::finish_data(const float coeff[9])
{
    bool success = true;
/*
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
*/
    success = set_calibration(coeff);

    // close
    XSync(display, False);

    printf("\t--> Making the calibration permanent <--\n");
    switch (output_type) {
        case OUTYPE_AUTO:
            // xorg.conf.d or alternatively xinput commands
            if (has_xorgconfd_support()) {
                success &= output_xorgconfd(coeff);
            } else {
                success &= output_xinput(coeff);
            }
            break;
        case OUTYPE_XORGCONFD:
            success &= output_xorgconfd(coeff);
            break;
        case OUTYPE_HAL:
            success &= output_hal(coeff);
            break;
        case OUTYPE_XINPUT:
            success &= output_xinput(coeff);
            break;
        default:
            fprintf(stderr, "ERROR: Evdev Calibrator does not support the supplied --output-type\n");
            success = false;
    }

    return success;
}

bool CalibratorLibinput::set_calibration(const float coeff[9]) {
    printf("\tSetting calibration data: {");
    for (int i = 0 ; i < 9 ; i++) {
        printf("%f", coeff[i]);
        if (i < 8 )
            printf(", ");
    }
    printf("}\n");

    try {
        setMatrix(LIBINPUTCALIBRATIONMATRIXPRO, coeff);
        reset_data = false;
    } catch(...) {
        if (verbose)
            printf("DEBUG: Failed to apply axis calibration.\n");
        return false;;
    }


    if (verbose)
            printf("DEBUG: Successfully applied axis calibration.\n");

    return true;
}

Atom CalibratorLibinput::xinput_parse_atom(const char *name)
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

XDeviceInfo* CalibratorLibinput::xinput_find_device_info(
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
bool CalibratorLibinput::xinput_do_set_int_prop( const char * name,
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

    prop = xinput_parse_atom(name);

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

bool CalibratorLibinput::output_xorgconfd(const float coeff[9])
{
/*
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
*/
    return true;
}

bool CalibratorLibinput::output_hal(const float coeff[9])
{
    return false;
}

bool CalibratorLibinput::output_xinput(const float coeff[9])
{
    /*
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
*/
    return true;
}

