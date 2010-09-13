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
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h> // strncpy, strlen

#ifdef HAVE_X11_XRANDR
// support for multi-head setups
#include <X11/extensions/Xrandr.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>

#include "calibrator.hh"


// Timeout parameters
const int time_step = 100;  // in milliseconds
const int max_time = 15000; // 5000 = 5 sec

// Clock appereance
const int cross_lines = 25;
const int cross_circle = 4;
const int clock_radius = 50;
const int clock_line_width = 10;

// Text printed on screen
const int font_size = 16;
const int help_lines = 4;
const std::string help_text[help_lines] = {
    "Touchscreen Calibration",
    "Press the point, use a stylus to increase precision.",
    "",
    "(To abort, press any key or wait)"
};

// color management
enum { BLACK=0, WHITE=1, GRAY=2, DIMGRAY=3, RED=4 };
const int nr_colors = 5;
const char* colors[nr_colors] = {"BLACK", "WHITE", "GRAY", "DIMGRAY", "RED"};


void sigalarm_handler(int num);


/*******************************************
 * X11 class for the the calibration GUI
 *******************************************/
class GuiCalibratorX11
{
public:
    GuiCalibratorX11(Calibrator* w);
    ~GuiCalibratorX11();
    static bool set_instance(GuiCalibratorX11* W);
    static void give_timer_signal();

protected:
    // Data
    Calibrator* calibrator;
    double X[4], Y[4];
    int display_width, display_height;
    int time_elapsed;

    // X11 vars
    Display* display;
    int screen_num;
    Window win;
    GC gc;
    XFontStruct* font_info;
    // color mngmt
    unsigned long pixel[nr_colors];


    // Signal handlers
    bool on_timer_signal();
    bool on_expose_event();
    bool on_button_press_event(XEvent event);

    // Helper functions
    void set_display_size(int width, int height);
    void redraw();
    void draw_message(const char* msg);

private:
    static GuiCalibratorX11* instance;
};
GuiCalibratorX11* GuiCalibratorX11::instance = NULL;


GuiCalibratorX11::GuiCalibratorX11(Calibrator* calibrator0)
  : calibrator(calibrator0), time_elapsed(0)
{
    display = XOpenDisplay(NULL);
    if (display == NULL) {
        throw std::runtime_error("Unable to connect to X server");
    }
    screen_num = DefaultScreen(display);
    // Load font and get font information structure
    font_info = XLoadQueryFont(display, "9x15");
    if (font_info == NULL) {
        // fall back to native font
        font_info = XLoadQueryFont(display, "fixed");
        if (font_info == NULL) {
            XCloseDisplay(display);
            throw std::runtime_error("Unable to open neither '9x15' nor 'fixed' font");
        }
    }

#ifdef HAVE_X11_XRANDR
    // get screensize from xrandr
    int nsizes;
    XRRScreenSize* randrsize = XRRSizes(display, screen_num, &nsizes);
    if (nsizes != 0) {
        set_display_size(randrsize->width, randrsize->height);
    } else {
        set_display_size(DisplayWidth(display, screen_num),
                         DisplayHeight(display, screen_num));
    }
# else
    set_display_size(DisplayWidth(display, screen_num),
                     DisplayHeight(display, screen_num));
#endif

    // Register events on the window
    XSetWindowAttributes attributes;
    attributes.override_redirect = True;
    attributes.event_mask = ExposureMask | KeyPressMask | ButtonPressMask;

    win = XCreateWindow(display, RootWindow(display, screen_num),
                0, 0, display_width, display_height, 0,
                CopyFromParent, InputOutput, CopyFromParent,
                CWOverrideRedirect | CWEventMask,
                &attributes);
    XMapWindow(display, win);

    // Listen to events
    XGrabKeyboard(display, win, False, GrabModeAsync, GrabModeAsync,
                CurrentTime);
    XGrabPointer(display, win, False, ButtonPressMask, GrabModeAsync,
                GrabModeAsync, None, None, CurrentTime);

    Colormap colormap = DefaultColormap(display, screen_num);
    XColor color;
    for (int i = 0; i != nr_colors; i++) {
        XParseColor(display, colormap, colors[i], &color);
        XAllocColor(display, colormap, &color);
        pixel[i] = color.pixel;
    }
    XSetWindowBackground(display, win, pixel[GRAY]);
    XClearWindow(display, win);

    gc = XCreateGC(display, win, 0, NULL);
    XSetFont(display, gc, font_info->fid);

    // Setup timer for animation
    signal(SIGALRM, sigalarm_handler);
    struct itimerval timer;
    timer.it_value.tv_sec = time_step/1000;
    timer.it_value.tv_usec = (time_step % 1000) * 1000;
    timer.it_interval = timer.it_value;
    setitimer(ITIMER_REAL, &timer, NULL);
}

GuiCalibratorX11::~GuiCalibratorX11()
{
    XUngrabPointer(display, CurrentTime);
    XUngrabKeyboard(display, CurrentTime);
    XFreeGC(display, gc);
    XCloseDisplay(display);
}

void GuiCalibratorX11::set_display_size(int width, int height) {
    display_width = width;
    display_height = height;

    // Compute absolute circle centers
    const int delta_x = display_width/num_blocks;
    const int delta_y = display_height/num_blocks;
    X[UL] = delta_x;                     Y[UL] = delta_y;
    X[UR] = display_width - delta_x - 1; Y[UR] = delta_y;
    X[LL] = delta_x;                     Y[LL] = display_height - delta_y - 1;
    X[LR] = display_width - delta_x - 1; Y[LR] = display_height - delta_y - 1;

    // reset calibration if already started
    calibrator->reset();
}

void GuiCalibratorX11::redraw()
{
#ifdef HAVE_X11_XRANDR
    // check that screensize did not change
    int nsizes;
    XRRScreenSize* randrsize = XRRSizes(display, screen_num, &nsizes);
    if (nsizes != 0 && (display_width != randrsize->width ||
                        display_height != randrsize->height)) {
        set_display_size(randrsize->width, randrsize->height);
    }
#endif

    // Print the text
    int text_height = font_info->ascent + font_info->descent;
    int text_width = -1;
    for (int i = 0; i != help_lines; i++) {
        text_width = std::max(text_width, XTextWidth(font_info,
            help_text[i].c_str(), help_text[i].length()));
    }

    int x = (display_width - text_width) / 2;
    int y = (display_height - text_height) / 2 - 60;
    XSetForeground(display, gc, pixel[BLACK]);
    XSetLineAttributes(display, gc, 2, LineSolid, CapRound, JoinRound);
    XDrawRectangle(display, win, gc, x - 10, y - (help_lines*text_height) - 10,
                text_width + 20, (help_lines*text_height) + 20);

    // Print help lines
    y -= 3;
    for (int i = help_lines-1; i != -1; i--) {
        int w = XTextWidth(font_info, help_text[i].c_str(), help_text[i].length());
        XDrawString(display, win, gc, x + (text_width-w)/2, y,
                help_text[i].c_str(), help_text[i].length());
        y -= text_height;
    }

    // Draw the points
    for (int i = 0; i <= calibrator->get_numclicks(); i++) {
        // set color: already clicked or not
        if (i < calibrator->get_numclicks())
            XSetForeground(display, gc, pixel[WHITE]);
        else
            XSetForeground(display, gc, pixel[RED]);
        XSetLineAttributes(display, gc, 1, LineSolid, CapRound, JoinRound);

        XDrawLine(display, win, gc, X[i] - cross_lines, Y[i],
                X[i] + cross_lines, Y[i]);
        XDrawLine(display, win, gc, X[i], Y[i] - cross_lines,
                X[i], Y[i] + cross_lines);
        XDrawArc(display, win, gc, X[i] - cross_circle, Y[i] - cross_circle,
                (2 * cross_circle), (2 * cross_circle), 0, 360 * 64);
    }

    // Draw the clock background
    XSetForeground(display, gc, pixel[DIMGRAY]);
    XSetLineAttributes(display, gc, 0, LineSolid, CapRound, JoinRound);
    XFillArc(display, win, gc, (display_width-clock_radius)/2, (display_height - clock_radius)/2,
                clock_radius, clock_radius, 0, 360 * 64);
}

bool GuiCalibratorX11::on_expose_event()
{
    redraw();

    return true;
}

bool GuiCalibratorX11::on_timer_signal()
{
    time_elapsed += time_step;
    if (time_elapsed > max_time) {
        exit(0);
    }

    // Update clock
    XSetForeground(display, gc, pixel[BLACK]);
    XSetLineAttributes(display, gc, clock_line_width,
                LineSolid, CapButt, JoinMiter);
    XDrawArc(display, win, gc, (display_width-clock_radius+clock_line_width)/2,
                (display_height-clock_radius+clock_line_width)/2,
                clock_radius-clock_line_width, clock_radius-clock_line_width,
                90*64, ((double)time_elapsed/(double)max_time) * -360 * 64);

    return true;
}

bool GuiCalibratorX11::on_button_press_event(XEvent event)
{
    // Clear window, maybe a bit overdone, but easiest for me atm.
    // (goal is to clear possible message and other clicks)
    XClearWindow(display, win);

    // Handle click
    time_elapsed = 0;
    bool success = calibrator->add_click(event.xbutton.x, event.xbutton.y);

    if (!success && calibrator->get_numclicks() == 0) {
        draw_message("Mis-click detected, restarting...");
    }

    // Are we done yet?
    if (calibrator->get_numclicks() >= 4) {
        // Recalibrate
        success = calibrator->finish(display_width, display_height);

        if (success) {
            exit(0);
        } else {
            // TODO, in GUI ?
            fprintf(stderr, "Error: unable to apply or save configuration values");
            exit(1);
        }
    }

    // Force a redraw
    redraw();

    return true;
}

void GuiCalibratorX11::draw_message(const char* msg)
{
    int text_height = font_info->ascent + font_info->descent;
    int text_width = XTextWidth(font_info, msg, strlen(msg));

    int x = (display_width - text_width) / 2;
    int y = (display_height - text_height) / 2 + clock_radius + 60;
    XSetForeground(display, gc, pixel[BLACK]);
    XSetLineAttributes(display, gc, 2, LineSolid, CapRound, JoinRound);
    XDrawRectangle(display, win, gc, x - 10, y - text_height - 10,
                text_width + 20, text_height + 25);

    XDrawString(display, win, gc, x, y, msg, strlen(msg));
}

void GuiCalibratorX11::give_timer_signal()
{
    if (instance != NULL) {
        // timer signal
        //check timeout
        instance->on_timer_signal();

        // process events
        XEvent event;
        while (XCheckWindowEvent(instance->display, instance->win, -1, &event) == True) {
            switch (event.type) {
                case Expose:
                    // only draw the last contiguous expose
                    if (event.xexpose.count != 0)
                        break;
                    instance->on_expose_event();
                    break;

                case ButtonPress:
                    instance->on_button_press_event(event);
                    break;

                case KeyPress:
                    exit(0);
                    break;
            }
        }
    }
}

bool GuiCalibratorX11::set_instance(GuiCalibratorX11* W)
{
    bool wasSet = (instance != NULL);
    instance = W;

    return wasSet;
}


// handle SIGALRM signal, pass to singleton
void sigalarm_handler(int num)
{
    if (num == SIGALRM) {
        GuiCalibratorX11::give_timer_signal();
    }
}
