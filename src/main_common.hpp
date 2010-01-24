/*
 * Copyright (c) 2009 Tias Guns
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
#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <stdexcept>

#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>

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

// Threshold to keep the same point from being clicked twice.
// Set to zero if you don't want this check
const int click_threshold = 7;

// Names of the points
enum {
    UL = 0, // Upper-left
    UR = 1, // Upper-right
    LL = 2, // Lower-left
    LR = 3  // Lower-right
};

// struct to hold min/max info of the X and Y axis
struct XYinfo {
    int x_min;
    int x_max;
    int y_min;
    int y_max;
    XYinfo() : x_min(-1), x_max(-1), y_min(-1), y_max(-1) {}
    XYinfo(int xmi, int xma, int ymi, int yma) :
         x_min(xmi), x_max(xma), y_min(ymi), y_max(yma) {}
};

class WrongCalibratorException : public std::invalid_argument {
    public:
        WrongCalibratorException(const std::string& msg = "") :
            std::invalid_argument(msg) {}
};

// all need struct XYinfo, and some the consts too
#include "calibrator.cpp"
#include "calibrator/calibratorXorgPrint.cpp"
#include "calibrator/calibratorEvdev.cpp"
#include "calibrator/calibratorUsbtouchscreen.cpp"



// find a calibratable device (using Xinput)
// retuns number of devices found,
// data of last driver is returned in the function parameters
int find_driver(bool verbose, const char*& drivername, XYinfo& axys);
int find_driver(bool verbose, const char*& drivername, XYinfo& axys)
{
    int found = 0;

    Display* display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "Unable to connect to X server\n");
        exit(1);
    }

    int xi_opcode, event, error;
    if (!XQueryExtension(display, "XInputExtension", &xi_opcode, &event, &error)) {
        fprintf(stderr, "X Input extension not available.\n");
        exit(1);
    }

    // verbose, get Xi version
    if (verbose) {
        XExtensionVersion *version = XGetExtensionVersion(display, INAME);

        if (version && (version != (XExtensionVersion*) NoSuchExtension)) {
            printf("DEBUG: %s version is %i.%i\n",
                INAME, version->major_version, version->minor_version);
            XFree(version);
        }
    }

    int ndevices;
    XDeviceInfoPtr list, slist;
    slist=list=(XDeviceInfoPtr) XListInputDevices (display, &ndevices);
    for (int i=0; i<ndevices; i++, list++)
    {
        if (list->use == IsXKeyboard || list->use == IsXPointer)
            continue;

        XAnyClassPtr any = (XAnyClassPtr) (list->inputclassinfo);
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
                    drivername = strdup(list->name);
                    axys.x_min = ax[0].min_value;
                    axys.x_max = ax[0].max_value;
                    axys.y_min = ax[1].min_value;
                    axys.y_max = ax[1].max_value;
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

    return found;
}

static void usage(char* cmd)
{
    fprintf(stderr, "xinput_calibratior, v%s\n\n", VERSION);
    fprintf(stderr, "Usage: %s [-h|--help] [-v|--verbose] [--precalib <minx> <maxx> <miny> <maxy>] [--fake]\n", cmd);
    fprintf(stderr, "\t-h, --help: print this help message\n");
    fprintf(stderr, "\t-v, --verbose: print debug messages during the process\n");
    fprintf(stderr, "\t--precalib: manually provide the current calibration setting (eg the values in xorg.conf)\n");
    fprintf(stderr, "\t--fake: emulate a fake driver (for testing purposes)\n");
}

Calibrator* main_common(int argc, char** argv);
Calibrator* main_common(int argc, char** argv)
{
    bool verbose = false;
    bool fake = false;
    bool precalib = false;
    XYinfo pre_axys;

    // parse input
    if (argc > 1) {
        for (int i=1; i!=argc; i++) {
            // Display help ?
            if (strcmp("-h", argv[i]) == 0 ||
                strcmp("--help", argv[i]) == 0) {
                usage(argv[0]);
                exit(0);
            } else

            // Verbose output ?
            if (strcmp("-v", argv[i]) == 0 ||
                strcmp("--verbose", argv[i]) == 0) {
                verbose = true;
            } else

            // Get pre-calibration ?
            if (strcmp("--precalib", argv[i]) == 0) {
                precalib = true;
                if (argc > i+1)
                    pre_axys.x_min = atoi(argv[i+1]);
                if (argc > i+2)
                    pre_axys.x_max = atoi(argv[i+2]);
                if (argc > i+3)
                    pre_axys.y_min = atoi(argv[i+3]);
                if (argc > i+4)
                    pre_axys.y_max = atoi(argv[i+4]);
            } else

            // Fake calibratable device ?
            if (strcmp("--fake", argv[i]) == 0) {
                fake = true;
            }
        }
    }
    
    // find driver(s)
    const char* drivername = NULL;
    XYinfo axys;
    if (fake) {
        // Fake a calibratable device
        drivername = "Fake_device";
        axys = XYinfo(0,0,0,0);

        if (verbose) {
            printf("DEBUG: Faking device: %s\n", drivername);
        }
    } else {
        // Find the right device
        int nr_found = find_driver(verbose, drivername, axys);

        if (nr_found == 0) {
            fprintf (stderr, "Error: No calibratable devices found.\n");
            exit(1);
        } else if (nr_found > 1) {
            printf ("Warning: multiple calibratable devices found, calibrating last one (%s)\n", drivername);
        }
    }

    // override min/max XY from command line ?
    if (precalib) {
        if (pre_axys.x_min != -1)
            axys.x_min = pre_axys.x_min;
        if (pre_axys.x_max != -1)
            axys.x_max = pre_axys.x_max;
        if (pre_axys.y_min != -1)
            axys.y_min = pre_axys.y_min;
        if (pre_axys.y_max != -1)
            axys.y_max = pre_axys.y_max;

        if (verbose) {
            printf("DEBUG: Setting precalibration: %i, %i, %i, %i\n",
                axys.x_min, axys.x_max, axys.y_min, axys.y_max);
        }
    }


    // Different device/driver, different calibrator usage
    try {
        // Usbtouchscreen driver
        return new CalibratorUsbtouchscreen(drivername, axys, verbose);

    } catch(WrongCalibratorException& x) {
        if (verbose)
            printf("DEBUG: Not usbtouchscreen calibrator: %s\n", x.what());
    }

    try {
        // next, try Evdev driver
        return new CalibratorEvdev(drivername, axys, verbose);

    } catch(WrongCalibratorException& x) {
        if (verbose)
            printf("DEBUG: Not evdev calibrator: %s\n", x.what());
    }

    // lastly, presume a standard Xorg driver (evtouch, mutouch, ...)
    return new CalibratorXorgPrint(drivername, axys, verbose);
}
