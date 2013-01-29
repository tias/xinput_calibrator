/*
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

#include "calibrator/Usbtouchscreen.hpp"

#include <cstdlib>
#include <cstdio>
#include <cstring>

/*************************
 * Variables for usbtouchscreen specifically
 *************************/
// The file to which the calibration parameters are saved.
// (XXX: is this distribution dependend?)
static const char *modprobe_conf_local = "/etc/modprobe.conf.local";

// Prefix to the kernel path where we can set the parameters
static const char *module_prefix = "/sys/module/usbtouchscreen/parameters";

// Names of kernel parameters
static const char *p_range_x = "range_x";
static const char *p_range_y = "range_y";
static const char *p_min_x = "min_x";
static const char *p_min_y = "min_y";
static const char *p_max_x = "max_x";
static const char *p_max_y = "max_y";
static const char *p_transform_xy = "transform_xy";
static const char *p_flip_x = "flip_x";
static const char *p_flip_y = "flip_y";
static const char *p_swap_xy = "swap_xy";

CalibratorUsbtouchscreen::CalibratorUsbtouchscreen(const char* const device_name0, const XYinfo& axys0, const int thr_misclick, const int thr_doubleclick, const OutputType output_type, const char* geometry, const bool use_timeout, const char* output_filename)
  : Calibrator(device_name0, axys0, thr_misclick, thr_doubleclick, output_type, geometry, use_timeout, output_filename)
{
    if (strcmp(device_name, "Usbtouchscreen") != 0)
        throw WrongCalibratorException("Not a usbtouchscreen device");

    // Reset the currently running kernel
    read_bool_parameter(p_transform_xy, val_transform_xy);
    read_bool_parameter(p_flip_x, val_flip_x);
    read_bool_parameter(p_flip_y, val_flip_y);
    read_bool_parameter(p_swap_xy, val_swap_xy);

    write_bool_parameter(p_transform_xy, false);
    write_bool_parameter(p_flip_x, false);
    write_bool_parameter(p_flip_y, false);
    write_bool_parameter(p_swap_xy, false);

    printf("Calibrating Usbtouchscreen, through the kernel module\n");
}

CalibratorUsbtouchscreen::~CalibratorUsbtouchscreen()
{
    // Dirty exit, so we restore the parameters of the running kernel
    write_bool_parameter (p_transform_xy, val_transform_xy);
    write_bool_parameter (p_flip_x, val_flip_x);
    write_bool_parameter (p_flip_y, val_flip_y);
    write_bool_parameter (p_swap_xy, val_swap_xy);
}

bool CalibratorUsbtouchscreen::finish_data(const XYinfo new_axys)
{
    if (output_type != OUTYPE_AUTO) {
        fprintf(stderr, "ERROR: Usbtouchscreen Calibrator does not support the supplied --output-type\n");
        return false;
    }

    // New ranges
    const int range_x = (new_axys.x.max - new_axys.x.min);
    const int range_y = (new_axys.y.max - new_axys.y.min);
    // Should x and y be flipped ?
    const bool flip_x = (new_axys.x.min > new_axys.x.max);
    const bool flip_y = (new_axys.y.min > new_axys.y.max);

    // Send the estimated parameters to the currently running kernel
    write_int_parameter(p_range_x, range_x);
    write_int_parameter(p_range_y, range_y);
    write_int_parameter(p_min_x, new_axys.x.min);
    write_int_parameter(p_max_x, new_axys.x.max);
    write_int_parameter(p_min_y, new_axys.y.min);
    write_int_parameter(p_max_y, new_axys.y.max);
    write_bool_parameter(p_transform_xy, true);
    write_bool_parameter(p_flip_x, flip_x);
    write_bool_parameter(p_flip_y, flip_y);
    write_bool_parameter(p_swap_xy, new_axys.swap_xy);

    // Read, then write calibration parameters to modprobe_conf_local,
    // or the file set by --output-filename to keep the for the next boot
    const char* filename = output_filename == NULL ? modprobe_conf_local : output_filename;
    FILE *fid = fopen(filename, "r");
    if (fid == NULL) {
        fprintf(stderr, "Error: Can't open '%s' for reading. Make sure you have the necessary rights\n", filename);
        fprintf(stderr, "New calibration data NOT saved\n");
        return false;
    }

    std::string new_contents;
    const int len = MAX_LINE_LEN;
    char line[len];
    const char *opt = "options usbtouchscreen";
    const int opt_len = strlen(opt);
    while (fgets(line, len, fid)) {
        if (strncmp(line, opt, opt_len) == 0) {
            // This is the line we want to remove
            continue;
        }
        new_contents += line;
    }
    fclose(fid);

    char *new_opt = new char[opt_len];
    sprintf(new_opt, "%s %s=%d %s=%d %s=%d %s=%d %s=%d %s=%d %s=%c %s=%c %s=%c %s=%c\n",
         opt, p_range_x, range_x, p_range_y, range_y,
         p_min_x, new_axys.x.min, p_min_y, new_axys.y.min,
         p_max_x, new_axys.x.max, p_max_y, new_axys.y.max,
         p_transform_xy, yesno(true), p_flip_x, yesno(flip_x),
         p_flip_y, yesno(flip_y), p_swap_xy, yesno(new_axys.swap_xy));
    new_contents += new_opt;

    fid = fopen(filename, "w");
    if (fid == NULL) {
        fprintf(stderr, "Error: Can't open '%s' for writing. Make sure you have the necessary rights\n", filename);
        fprintf(stderr, "New calibration data NOT saved\n");
        return false;
    }
    fprintf(fid, "%s", new_contents.c_str ());
    fclose(fid);

    return true;
}

void CalibratorUsbtouchscreen::read_int_parameter(const char *param, int &value)
{
    int dummy;
    char filename[100];
    sprintf(filename, "%s/%s", module_prefix, param);
    FILE *fid = fopen(filename, "r");
    if (fid == NULL) {
        fprintf(stderr, "Could not read parameter '%s'\n", param);
        return;
    }

    dummy = fscanf(fid, "%d", &value);
    fclose(fid);
}

void CalibratorUsbtouchscreen::read_bool_parameter(const char *param, bool &value)
{
    char *dummy;
    char filename[100];
    sprintf(filename, "%s/%s", module_prefix, param);
    FILE *fid = fopen(filename, "r");
    if (fid == NULL) {
        fprintf(stderr, "Could not read parameter '%s'\n", param);
        return;
    }

    char val[3];
    dummy = fgets(val, 2, fid);
    fclose(fid);
        value = (val[0] == yesno(true));
}

void CalibratorUsbtouchscreen::write_int_parameter(const char *param, const int value)
{
    char filename[100];
    sprintf(filename, "%s/%s", module_prefix, param);
    FILE *fid = fopen(filename, "w");
    if (fid == NULL) {
        fprintf(stderr, "Could not save parameter '%s'\n", param);
        return;
    }

    fprintf(fid, "%d", value);
    fclose(fid);
}

void CalibratorUsbtouchscreen::write_bool_parameter(const char *param, const bool value)
{
    char filename[100];
    sprintf(filename, "%s/%s", module_prefix, param);
    FILE *fid = fopen(filename, "w");
    if (fid == NULL) {
        fprintf(stderr, "Could not save parameter '%s'\n", param);
        return;
    }

    fprintf(fid, "%c", yesno (value));
    fclose(fid);
}
