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

#include <iostream>
#include <eigen3/Eigen/Dense>
using namespace Eigen;

void getTransMatrix(float *res, float w, float h, int num_blocks,
		    float x1, float y1,
		    float x2, float y2,
		    float x3, float y3,
		    float x4, float y4
) {
        Matrix<float,4,3> s;
        Matrix<float,4,3> p;
        p << x1,y1,1, x2,y2,1, x3,y3,1, x4,y4,1;
        s << w/(float)num_blocks,h/(float)num_blocks,1, 
		w*(num_blocks-1)/(float)num_blocks,h/(float)num_blocks,1, 
		w/(float)num_blocks,h*(num_blocks-1)/(float)num_blocks,1, 
		w*(num_blocks-1)/(float)num_blocks,h*(num_blocks-1)/(float)num_blocks,1;
        Matrix<float,3,3> tmp = (p.transpose() * p).ldlt().solve(p.transpose() * s);

        std::cout << tmp.transpose() << std::endl;
	res[0] = tmp(0,0);
	res[1] = tmp(1,0);
	res[2] = tmp(2,0)/w;
	res[3] = tmp(0,1);
	res[4] = tmp(1,1);
	res[5] = tmp(2,1)/h;
	res[6] = 0.0;
	res[7] = 0.0;
	res[8] = 1.0;
}


#include "calibrator/Matrix.hpp"

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
CalibratorMatrix::CalibratorMatrix(const char* const device_name0,
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
		throw WrongCalibratorException("Matrix: Unable to connect to X server");
	}
	Atom float_atom = XInternAtom(display, "FLOAT", True);
	
	
	// normaly, we already have the device id
	if (device_id == (XID)-1) {
		devInfo = xinput_find_device_info(display, device_name, False);
		if (!devInfo) {
			XCloseDisplay(display);
			throw WrongCalibratorException("Matrix: Unable to find device");
		}
		device_id = devInfo->id;
	}
	
	dev = XOpenDevice(display, device_id);
	if (!dev) {
		XCloseDisplay(display);
		throw WrongCalibratorException("Matrix: Unable to open device");
	}
	
	// XGetDeviceProperty vars
	Atom            property;
	Atom            act_type;
	int             act_format;
	unsigned long   nitems, bytes_after;
	unsigned char   *data;
	
	// get "Matrix Axis Calibration" property
	property = xinput_parse_atom(display, "libinput Calibration Matrix");
	if (XGetDeviceProperty(display, dev, property, 0, 1000, False,
		AnyPropertyType, &act_type, &act_format,
		&nitems, &bytes_after, &data) != Success)
	{
		XCloseDevice(display, dev);
		XCloseDisplay(display);
		throw WrongCalibratorException("Matrix: \"libinput Calibration Matrix\" property missing, not a (valid) evdev device");
		
	} else {
		if (act_format != 32 || act_type != float_atom) {
			XCloseDevice(display, dev);
			XCloseDisplay(display);
			throw WrongCalibratorException("Matrix: invalid \"libinput Calibration Matrix\" property format");
		}
		//TODO: there's probably more verification to be made
		XFree(data);
	}
	//TODO: reinit the matrix
	int argc;
	float argv[9];
	if (!xinput_do_get_float_prop("libinput Calibration Matrix",display,&argc,argv) || argc != 9) {
		throw WrongCalibratorException("Matrix: \"libinput Calibration Matrix\" failed to read the old values");
	}
	float dm[9]; dm[0]=dm[4]=dm[8]=1.0;dm[1]=dm[2]=dm[3]=dm[5]=dm[6]=dm[7]=0.0;
	if (!xinput_do_set_float_prop("libinput Calibration Matrix",display, 9,dm)) {
		throw WrongCalibratorException("Matrix: \"libinput Calibration Matrix\" failed to set the default matrix");
	}
}
// protected pass-through constructor for subclasses
CalibratorMatrix::CalibratorMatrix(const char* const device_name0,
				   const XYinfo& axys0,
				   const int thr_misclick,
				   const int thr_doubleclick,
				   const OutputType output_type,
				   const char* geometry,
				   const bool use_timeout,
				   const char* output_filename)
: Calibrator(device_name0, axys0, thr_misclick, thr_doubleclick, output_type, geometry, output_filename) { }

// Destructor
CalibratorMatrix::~CalibratorMatrix () {
	XCloseDevice(display, dev);
	XCloseDisplay(display);
}

// From Calibrator but with evdev specific invertion option
// KEEP IN SYNC with Calibrator::finish() !!
bool CalibratorMatrix::finish(int width, int height)
{
	if (get_numclicks() != NUM_POINTS) {
		return false;
	}
	this->width = width;
	this->height = height;
	
	// new axis origin and scaling
	// based on old_axys: inversion/swapping is relative to the old axis
	XYinfo new_axis(old_axys);
	
	
	// calculate average of clicks
	float x_min = (clicked.x[UL] + clicked.x[LL])/2.0;
	float x_max = (clicked.x[UR] + clicked.x[LR])/2.0;
	float y_min = (clicked.y[UL] + clicked.y[UR])/2.0;
	float y_max = (clicked.y[LL] + clicked.y[LR])/2.0;
	
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
	
	// round and put in new_axis struct
	new_axis.x.min = round(x_min); new_axis.x.max = round(x_max);
	new_axis.y.min = round(y_min); new_axis.y.max = round(y_max);

	getTransMatrix(final_matrix, width, height, num_blocks,
		(float)clicked.x[UL], (float)clicked.y[UL],
		(float)clicked.x[UR], (float)clicked.y[UR],
		(float)clicked.x[LL], (float)clicked.y[LL],
		(float)clicked.x[LR], (float)clicked.y[LR]);
	
	// finish the data, driver/calibrator specific
	return finish_data(new_axis);
}

// Activate calibrated data and output it
bool CalibratorMatrix::finish_data(const XYinfo &new_axys)
{
	const char* sysfs_name = get_sysfs_name();
	bool not_sysfs_name = (sysfs_name == NULL);

	bool success = true;
	
#if 0
	float tm[9];
	tm[1] = tm[3] = tm[6] = tm[7] = 0.0;
	tm[8] = 1.0;
	tm[0] = (float)width/((float)(new_axys.x.max-new_axys.x.min));
	tm[2] = ((float)width - tm[0]*new_axys.x.max)/(float)width;
	tm[4] = (float)height/((float)(new_axys.y.max-new_axys.y.min));
	tm[5] = ((float)height - tm[4]*new_axys.y.max)/(float)height;
#else
	float *tm = this->final_matrix;
#endif
	printf("\nDoing dynamic recalibration:\n");
	if (!xinput_do_set_float_prop("libinput Calibration Matrix",display, 9,tm)) {
		fprintf(stderr, "Matrix: \"libinput Calibration Matrix\" failed to set the new matrix");
	}	

	printf("\t--> Making the calibration permanent <--\n");
	char line[MAX_LINE_LEN];
	std::string outstr;
	
	outstr = "Section \"InputClass\"\n";
	sprintf(line, "	Identifier	\"%s\"\n", sysfs_name);
	outstr += line;
	sprintf(line, "	MatchProduct	\"%s\"\n", sysfs_name);
	outstr += line;
	sprintf(line, "	Option	\"CalibrationMatrix\"	\"%f %f %f %f %f %f %f %f %f\"\n",
		tm[0],tm[1],tm[2],tm[3],tm[4],tm[5],tm[6],tm[7],tm[8]);
	outstr += line;
	outstr += "EndSection\n";
	
	// console out
	printf("%s", outstr.c_str());
	if (not_sysfs_name)
		printf("\nChange '%s' to your device's name in the snippet above.\n", sysfs_name);

	return success;
}

Atom CalibratorMatrix::xinput_parse_atom(Display *display, const char *name)
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

XDeviceInfo* CalibratorMatrix::xinput_find_device_info(
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

bool CalibratorMatrix::xinput_do_set_float_prop( const char * name,
						 Display *display,
						 int argc,
						 const float* argv)
{
	#ifndef HAVE_XI_PROP
	return false;
	#else
	
	Atom          prop;
	int           i;
	
	union {
		unsigned char *c;
		long *l;
		float *f;
	} data;
	Atom float_atom = XInternAtom(display, "FLOAT", True);
	
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
	
	data.c = (unsigned char*)calloc(argc, sizeof(float));
	
	for (i = 0; i < argc; i++) {
		*(float *)(data.l + i) = argv[i];
	}
	
	XChangeDeviceProperty(display, dev, prop, float_atom, 32, PropModeReplace,
			      data.c, argc);
	free(data.c);
	XSync(display, False);
	return true;
	#endif // HAVE_XI_PROP
}

bool CalibratorMatrix::xinput_do_get_float_prop( const char * pname,
						 Display *display,
						 int* argc,
						 float* argv)
{
	Atom                act_type;
	Atom          	property;
	int                 act_format;
	unsigned long       nitems, bytes_after;
	unsigned char       *ptr, *data;
	int                 j, size = 0;
	
	property = xinput_parse_atom(display, pname);
	
	if (XGetDeviceProperty(display, dev, property, 0, 1000, False,
		AnyPropertyType, &act_type, &act_format,
		&nitems, &bytes_after, &data) == Success)
	{
		ptr = data;
		*argc = nitems;
		switch(act_format)
		{
			case 8: size = sizeof(char); break;
			case 16: size = sizeof(short); break;
			case 32: size = sizeof(long); break;
		}
		
		for (j = 0; j < *argc; j++)
		{
			argv[j] = *((float*)ptr);
			ptr += size;
		}
		XFree(data);
		return true;
	} else
		return false;
	
}


