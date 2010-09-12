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

// Names of the points
enum {
    UL = 0, // Upper-left
    UR = 1, // Upper-right
    LL = 2, // Lower-left
    LR = 3  // Lower-right
};

// Output types
enum OutputType {
    OUTYPE_AUTO,
    OUTYPE_XORGCONFD,
    OUTYPE_HAL,
    OUTYPE_XINPUT
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

// strdup: non-ansi
char* my_strdup(const char* s);
char* my_strdup(const char* s) {
    size_t len = strlen(s) + 1;
    void* p = malloc(len);

    if (p == NULL)
        return NULL;

    return (char*) memcpy(p, s, len);
}

// all need struct XYinfo, and some the consts too
#include "calibrator.cpp"
#include "calibrator/calibratorXorgPrint.cpp"
#include "calibrator/calibratorEvdev.cpp"
#include "calibrator/calibratorUsbtouchscreen.cpp"


/**
 * find a calibratable touchscreen device (using XInput)
 *
 * if pre_device is NULL, the last calibratable device is selected.
 * retuns number of devices found,
 * the data of the device is returned in the last 3 function parameters
 */
int find_device(const char*, bool, bool, XID&, const char*&, XYinfo&);
int find_device(const char* pre_device, bool verbose, bool list_devices,
        XID& device_id, const char*& device_name, XYinfo& device_axys)
{
    bool pre_device_is_id = true;
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

    if (pre_device != NULL) {
        // check whether the pre_device is an ID (only digits)
        int len = strlen(pre_device);
        for (int loop=0; loop<len; loop++) {
	        if (!isdigit(pre_device[loop])) {
	            pre_device_is_id = false;
	            break;
	        }
        }
    }


    if (verbose)
        printf("DEBUG: Skipping virtual master devices and devices without axis valuators.\n");
    int ndevices;
    XDeviceInfoPtr list, slist;
    slist=list=(XDeviceInfoPtr) XListInputDevices (display, &ndevices);
    for (int i=0; i<ndevices; i++, list++)
    {
        if (list->use == IsXKeyboard || list->use == IsXPointer) // virtual master device
            continue;

        // if we are looking for a specific device
        if (pre_device != NULL) {
            if ((pre_device_is_id && list->id == (XID) atoi(pre_device)) ||
                (!pre_device_is_id && strcmp(list->name, pre_device) == 0)) {
                // OK, fall through
            } else {
                // skip, not this device
                continue;
            }
        }

        XAnyClassPtr any = (XAnyClassPtr) (list->inputclassinfo);
        for (int j=0; j<list->num_classes; j++)
        {

            if (any->c_class == ValuatorClass)
            {
                XValuatorInfoPtr V = (XValuatorInfoPtr) any;
                XAxisInfoPtr ax = (XAxisInfoPtr) V->axes;

                if (V->mode != Absolute) {
                    if (verbose)
                        printf("DEBUG: Skipping device '%s' id=%i, does not report Absolute events.\n",
                            list->name, (int)list->id);
                } else if (V->num_axes < 2 ||
                    (ax[0].min_value == -1 && ax[0].max_value == -1) ||
                    (ax[1].min_value == -1 && ax[1].max_value == -1)) {
                    if (verbose)
                        printf("DEBUG: Skipping device '%s' id=%i, does not have two calibratable axes.\n",
                            list->name, (int)list->id);
                } else {
                    /* a calibratable device (has 2 axis valuators) */
                    found++;
                    device_id = list->id;
                    device_name = my_strdup(list->name);
                    device_axys.x_min = ax[0].min_value;
                    device_axys.x_max = ax[0].max_value;
                    device_axys.y_min = ax[1].min_value;
                    device_axys.y_max = ax[1].max_value;

                    if (list_devices)
                        printf("Device \"%s\" id=%i\n", device_name, (int)device_id);
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

static void usage(char* cmd, unsigned thr_misclick)
{
    fprintf(stderr, "Usage: %s [-h|--help] [-v|--verbose] [--list] [--device <device name or id>] [--precalib <minx> <maxx> <miny> <maxy>] [--misclick <nr of pixels>] [--output-type <auto|xorg.conf.d|hal|xinput>] [--fake]\n", cmd);
    fprintf(stderr, "\t-h, --help: print this help message\n");
    fprintf(stderr, "\t-v, --verbose: print debug messages during the process\n");
    fprintf(stderr, "\t--list: list calibratable input devices and quit\n");
    fprintf(stderr, "\t--device <device name or id>: select a specific device to calibrate\n");
    fprintf(stderr, "\t--precalib: manually provide the current calibration setting (eg. the values in xorg.conf)\n");
    fprintf(stderr, "\t--misclick: set the misclick threshold (0=off, default: %i pixels)\n",
        thr_misclick);
    fprintf(stderr, "\t--output-type <auto|xorg.conf.d|hal|xinput>: type of config to ouput (auto=automatically detect, default: auto)\n");
    fprintf(stderr, "\t--fake: emulate a fake device (for testing purposes)\n");
}

Calibrator* main_common(int argc, char** argv);
Calibrator* main_common(int argc, char** argv)
{
    bool verbose = false;
    bool list_devices = false;
    bool fake = false;
    bool precalib = false;
    XYinfo pre_axys;
    const char* pre_device = NULL;
    unsigned thr_misclick = 15;
    unsigned thr_doubleclick = 7;
    OutputType output_type = OUTYPE_AUTO;

    // parse input
    if (argc > 1) {
        for (int i=1; i!=argc; i++) {
            // Display help ?
            if (strcmp("-h", argv[i]) == 0 ||
                strcmp("--help", argv[i]) == 0) {
                fprintf(stderr, "xinput_calibratior, v%s\n\n", VERSION);
                usage(argv[0], thr_misclick);
                exit(0);
            } else

            // Verbose output ?
            if (strcmp("-v", argv[i]) == 0 ||
                strcmp("--verbose", argv[i]) == 0) {
                verbose = true;
            } else

            // Just list devices ?
            if (strcmp("--list", argv[i]) == 0) {
                list_devices = true;
            } else

            // Select specific device ?
            if (strcmp("--device", argv[i]) == 0) {
                if (argc > i+1)
                    pre_device = argv[++i];
                else {
                    fprintf(stderr, "Error: --device needs a device name or id as argument; use --list to list the calibratable input devices.\n\n");
                    usage(argv[0], thr_misclick);
                    exit(1);
                }
            } else

            // Get pre-calibration ?
            if (strcmp("--precalib", argv[i]) == 0) {
                precalib = true;
                if (argc > i+1)
                    pre_axys.x_min = atoi(argv[++i]);
                if (argc > i+1)
                    pre_axys.x_max = atoi(argv[++i]);
                if (argc > i+1)
                    pre_axys.y_min = atoi(argv[++i]);
                if (argc > i+1)
                    pre_axys.y_max = atoi(argv[++i]);
            } else

            // Get mis-click threshold ?
            if (strcmp("--misclick", argv[i]) == 0) {
                if (argc > i+1)
                    thr_misclick = atoi(argv[++i]);
                else {
                    fprintf(stderr, "Error: --misclick needs a number (the pixel threshold) as argument. Set to 0 to disable mis-click detection.\n\n");
                    usage(argv[0], thr_misclick);
                    exit(1);
                }
            } else

            // Get output type ?
            if (strcmp("--output-type", argv[i]) == 0) {
                if (argc > i+1) {
                    i++; // eat it or exit
                    if (strcmp("auto", argv[i]) == 0)
                        output_type = OUTYPE_AUTO;
                    else if (strcmp("xorg.conf.d", argv[i]) == 0)
                        output_type = OUTYPE_XORGCONFD;
                    else if (strcmp("hal", argv[i]) == 0)
                        output_type = OUTYPE_HAL;
                    else if (strcmp("xinput", argv[i]) == 0)
                        output_type = OUTYPE_XINPUT;
                    else {
                        fprintf(stderr, "Error: --output-type needs one of auto|xorg.conf.d|hal|xinput.\n\n");
                        usage(argv[0], thr_misclick);
                        exit(1);
                    }
                } else {
                    fprintf(stderr, "Error: --output-type needs one argument.\n\n");
                    usage(argv[0], thr_misclick);
                    exit(1);
                }
            } else

            // Fake calibratable device ?
            if (strcmp("--fake", argv[i]) == 0) {
                fake = true;
            }
            
            // unknown option
            else {
                fprintf(stderr, "Unknown option: %s\n\n", argv[i]);
                usage(argv[0], thr_misclick);
                exit(0);
            }
        }
    }
    

    // Choose the device to calibrate
    XID         device_id   = (XID) -1;
    const char* device_name = NULL;
    XYinfo      device_axys;
    if (fake) {
        // Fake a calibratable device
        device_name = "Fake_device";
        device_axys = XYinfo(0,1000,0,1000);

        if (verbose) {
            printf("DEBUG: Faking device: %s\n", device_name);
        }
    } else {
        // Find the right device
        int nr_found = find_device(pre_device, verbose, list_devices, device_id, device_name, device_axys);

        if (list_devices) {
            // printed the list in find_device
            if (nr_found == 0)
                printf("No calibratable devices found.\n");
            exit(0);
        }

        if (nr_found == 0) {
            if (pre_device == NULL)
                fprintf (stderr, "Error: No calibratable devices found.\n");
            else
                fprintf (stderr, "Error: Device \"%s\" not found; use --list to list the calibratable input devices.\n", pre_device);
            exit(1);

        } else if (nr_found > 1) {
            printf ("Warning: multiple calibratable devices found, calibrating last one (%s)\n\tuse --device to select another one.\n", device_name);
        }

        if (verbose) {
            printf("DEBUG: Selected device: %s\n", device_name);
        }
    }

    // override min/max XY from command line ?
    if (precalib) {
        if (pre_axys.x_min != -1)
            device_axys.x_min = pre_axys.x_min;
        if (pre_axys.x_max != -1)
            device_axys.x_max = pre_axys.x_max;
        if (pre_axys.y_min != -1)
            device_axys.y_min = pre_axys.y_min;
        if (pre_axys.y_max != -1)
            device_axys.y_max = pre_axys.y_max;

        if (verbose) {
            printf("DEBUG: Setting precalibration: %i, %i, %i, %i\n",
                device_axys.x_min, device_axys.x_max,
                device_axys.y_min, device_axys.y_max);
        }
    }


    // Different device/driver, different ways to apply the calibration values
    try {
        // try Usbtouchscreen driver
        return new CalibratorUsbtouchscreen(device_name, device_axys,
            verbose, thr_misclick, thr_doubleclick, output_type);

    } catch(WrongCalibratorException& x) {
        if (verbose)
            printf("DEBUG: Not usbtouchscreen calibrator: %s\n", x.what());
    }

    try {
        // next, try Evdev driver (with XID)
        return new CalibratorEvdev(device_name, device_axys, verbose, device_id,
            thr_misclick, thr_doubleclick, output_type);

    } catch(WrongCalibratorException& x) {
        if (verbose)
            printf("DEBUG: Not evdev calibrator: %s\n", x.what());
    }

    // lastly, presume a standard Xorg driver (evtouch, mutouch, ...)
    return new CalibratorXorgPrint(device_name, device_axys,
            verbose, thr_misclick, thr_doubleclick, output_type);
}
