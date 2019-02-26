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

static void mat9_invert(const Mat9 &m, Mat9 &minv) {
    /*
     * from https://stackoverflow.com/questions/983999/simple-3x3-matrix-inverse-code-c
     * with some simplification
     */
    const float m4857 = m[4] * m[8] - m[5] * m[7];
    const float m3746 = m[3] * m[7] - m[4] * m[6];
    const float m5638 = m[5] * m[6] - m[3] * m[8];
    const float det = m[0] * (m4857) +
                 m[1] * (m5638) +
                 m[2] * (m3746);

    const float invdet = 1 / det;

    //Matrix33d minv; // inverse of matrix m
    minv[0] = (m4857) * invdet;
    minv[1] = (m[2] * m[7] - m[1] * m[8]) * invdet;
    minv[2] = (m[1] * m[5] - m[2] * m[4]) * invdet;
    minv[3] = (m5638) * invdet;
    minv[4] = (m[0] * m[8] - m[2] * m[6]) * invdet;
    minv[5] = (m[2] * m[3] - m[0] * m[5]) * invdet;
    minv[6] = (m3746) * invdet;
    minv[7] = (m[1] * m[6] - m[0] * m[7]) * invdet;
    minv[8] = (m[0] * m[4] - m[1] * m[3]) * invdet;
}

static void mat9_product(const Mat9 &m1, const Mat9 &m2, Mat9 &m3){
    int i,j, k;
    for (i = 0 ; i < 3 ; i++) {
        for (j = 0 ; j < 3 ; j++) {
            float sum = 0;
            for (k = 0 ; k < 3 ; k++)
                sum += m1[i*3+k]*m2[j+k*3];
            m3[i*3+j] = sum;
        }
    }
}

static void mat9_sum(const Mat9 &m1, Mat9 &m2){
    int i;
    for (i = 0 ; i < 9 ; i++)
        m2[i] += m1[i];
}

static void mat9_product(const float c, Mat9 &m1){
    int i;
    for (i = 0 ; i < 9 ; i++)
        m1[i] *= c;
}


static void mat9_print(const Mat9 &m) {
    int i,j;
    for (i = 0 ; i < 3 ; i++ ) {
        printf("\t[");
        for (j = 0 ; j < 3 ; j++) {
            if (j != 0)
                printf(", ");
            printf("%f", m[i*3+j]);
        }
        printf("]\n");
    }
}

static void mat9_set_identity(Mat9 &m) {
    static const float id[] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
    memcpy(m.coeff, id, sizeof(float) * 9);
}

void CalibratorLibinput::getMatrix(const char *name, Mat9 &coeff) {

    Atom float_atom = XInternAtom(display, "FLOAT", false);

    // get "Evdev Axis Calibration" property
    Atom property = xinput_parse_atom(name);

    Atom            act_type;
    int             act_format;
    unsigned long   nitems, bytes_after;
    unsigned char   *data, *data_ptr;

    if (XGetDeviceProperty(display, dev, property, 0, 1000, False,
                           AnyPropertyType, &act_type, &act_format,
                           &nitems, &bytes_after, &data) != Success) {
        throw WrongCalibratorException("Libinput: \"libinput Calibration Matrix\" property missing, not a (valid) libinput device");
    }

    if (act_type != float_atom || act_format != 32) {
        XFree(data);
        throw WrongCalibratorException("Libinput: \"libinput Calibration Matrix\" property format");

    }

    int size = sizeof(long);
    data_ptr = data;
    mat9_set_identity(coeff);
    for (unsigned int i = 0 ; i < nitems ; i++) {
        coeff[i] = *((float *)data_ptr);
        data_ptr += size;
    }

    XFree(data);
}

void CalibratorLibinput::setMatrix(const char *name, const Mat9 &coeff) {

    Atom          prop;
    int           format;
    long         *ptr;

    prop = xinput_parse_atom(name);
    if (prop == None) {
        fprintf(stderr, "invalid property '%s'\n", name);
        throw WrongCalibratorException("Libinput: \"libinput Calibration Matrix\" property missing, not a (valid) libinput device");
    }

    Atom float_atom = XInternAtom(display, "FLOAT", false);
    format = 32;
    int nelements = 9;
    ptr = (long*)calloc(nelements+20, sizeof(long));
    assert(ptr);

    for (int i = 0; i < nelements; i++)
         *((float*)(ptr+i)) = coeff[i];

    XChangeDeviceProperty(display, dev, prop, float_atom, format,
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
    throw WrongCalibratorException("Libinput: you need at least libXi 1.2 and inputproto 1.5 for dynamic recalibration of evdev.");
#else

    getMatrix(LIBINPUTCALIBRATIONMATRIXPRO, old_coeff);
    reset_data = true;

    /* FIXME, allow to override initial coeff */
    Mat9 coeff;
    mat9_set_identity(coeff);
    setMatrix(LIBINPUTCALIBRATIONMATRIXPRO, coeff);

    getMatrix(LIBINPUTCALIBRATIONMATRIXPRO, coeff);

    printf("Calibrating Libinput driver for \"%s\" id=%i\n",
            device_name, (int)device_id);
    printf("\tcurrent calibration values (from XInput):\n");
    mat9_print(coeff);

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
            //setMatrix(LIBINPUTCALIBRATIONMATRIXPRO, old_coeff);
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
/*
    [a  b  c]     [tx]     [sx]
    [d  e  f]  x  [ty]  =  [sy]
    [0  0  1]     [1 ]     [ 1]

        ^          ^        ^
        C          T        S

    Where:
        - a,b ...f -> conversion matrix
        - tx,ty    -> touch x,y
        - sx, sy   -> screen x,y


    C x [T1, T2, T3] = [S1, S2, S3]

*/

    Mat9 coeff_tmp, tmi, tm, ts, coeff;

    const float xl = width /  (float)num_blocks;
    const float xr = width /  (float)num_blocks * (num_blocks - 1);
    const float yu = height / (float)num_blocks;
    const float yl = height / (float)num_blocks * (num_blocks - 1);

    /* skip LR */
    tm.set(clicked.x[UL],   clicked.x[UR],  clicked.x[LL],
           clicked.y[UL],   clicked.y[UR],  clicked.y[LL],
           1,               1,              1);
    ts.set(xl,              xr,             xl,
           yu,              yu,             yl,
           1,               1,              1);

    mat9_invert(tm, tmi);
    mat9_product(ts, tmi, coeff);

    /* skip UL */
    tm.set(clicked.x[LR],   clicked.x[UR],  clicked.x[LL],
           clicked.y[LR],   clicked.y[UR],  clicked.y[LL],
           1,               1,              1);
    ts.set(xr,              xr,             xl,
           yl,              yu,             yl,
           1,               1,              1);

    mat9_invert(tm, tmi);
    mat9_product(ts, tmi, coeff_tmp);
    mat9_sum(coeff_tmp, coeff);

    /* skip UR */
    tm.set(clicked.x[LR],   clicked.x[UL],  clicked.x[LL],
           clicked.y[LR],   clicked.y[UL],  clicked.y[LL],
           1,               1,              1);
    ts.set(xr,              xl,             xl,
           yl,              yu,             yl,
           1,               1,              1);

    mat9_invert(tm, tmi);
    mat9_product(ts, tmi, coeff_tmp);
    mat9_sum(coeff_tmp, coeff);

    /* skip LL */
    tm.set(clicked.x[LR],   clicked.x[UL],  clicked.x[UR],
           clicked.y[LR],   clicked.y[UL],  clicked.y[UR],
           1,               1,              1);
    ts.set(xr,              xl,             xr,
           yl,              yu,             yu,
           1,               1,              1);

    mat9_invert(tm, tmi);
    mat9_product(ts, tmi, coeff_tmp);
    mat9_sum(coeff_tmp, coeff);

    mat9_product(1.0/4.0, coeff);

    /*
     * Normlize the coefficients
     */
    /* coeff[0] *= (float)width/width; */
    coeff[1] *= (float)height/width;
    coeff[2] *= 1.0/width;

    coeff[3] *= (float)width/height;
    /* coeff[4] *= (float)height/height; */
    coeff[5] *= 1.0/height;
    /*
     * Sometimes, the last row values -0.0, -0.0, 1
     * update to the right values !
     */
    coeff[6] = 0.0;
    coeff[7] = 0.0;
    coeff[8] = 1.0;

    return finish_data(coeff);
}

/*
 * this function assumes that the calibration must be performed via XYinfo;
 * however this is not true for libinput. Implement it to make the compiler
 * happy; however we must be sure that this is never called
 */
bool CalibratorLibinput::finish_data(const XYinfo &new_axys) {
    assert(0);
    return false;
}

// Activate calibrated data and output it
bool CalibratorLibinput::finish_data(const Mat9 &coeff)
{
    bool success = true;

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

bool CalibratorLibinput::set_calibration(const Mat9 &coeff) {
    printf("\tSetting calibration data: {");
    for (int i = 0 ; i < 9 ; i++) {
        printf("%f", coeff[i]);
        if (i < 8 )
            printf(", ");
    }
    printf("}\n");

    try {
        //setMatrix(LIBINPUTCALIBRATIONMATRIXPRO, coeff);
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

#if 0
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
#endif

bool CalibratorLibinput::output_xorgconfd(const Mat9 &coeff)
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
    sprintf(line, "	Option	\"CalibrationMatrix\"	\"%f %f %f %f %f %f %f %f %f \"\n",
                coeff[0], coeff[1], coeff[2], coeff[3], coeff[4], coeff[5],
                coeff[6], coeff[7], coeff[8]);
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

bool CalibratorLibinput::output_hal(const Mat9 &coeff)
{
    return false;
}

bool CalibratorLibinput::output_xinput(const Mat9 &coeff)
{

    if(output_filename == NULL)
        printf("  Install the 'xinput' tool and copy the command(s) below in a script that starts with your X session\n");
    else
        printf("  writing calibration script to '%s'\n", output_filename);

    // create startup script
    char line[MAX_LINE_LEN];
    std::string outstr;

    sprintf(line, "    xinput set-float-prop \"%s\" \""
                LIBINPUTCALIBRATIONMATRIXPRO
                "\" %f %f %f %f %f %f %f %f %f\n", device_name,
                coeff[0], coeff[1], coeff[2], coeff[3], coeff[4], coeff[5],
                coeff[6], coeff[7], coeff[8]
                           );
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

