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

// Must be before Xlib stuff
#include <gtkmm/main.h>
#include <gtkmm/window.h>
#include <cairomm/context.h>

#include "calibrator.hh"
#include "gui/gtkmm.hpp"

int main(int argc, char** argv)
{
    Calibrator* calibrator = Calibrator::make_calibrator(argc, argv);

    // GTK-mm setup
    Gtk::Main kit(argc, argv);

    Glib::RefPtr< Gdk::Screen > screen = Gdk::Screen::get_default();
    //int num_monitors = screen->get_n_monitors(); TODO, multiple monitors?
    Gdk::Rectangle rect;
    screen->get_monitor_geometry(0, rect);

    Gtk::Window win;
    // when no window manager: explicitely take size of full screen
    win.move(rect.get_x(), rect.get_y());
    win.resize(rect.get_width(), rect.get_height());
    // in case of window manager: set as full screen to hide window decorations
    win.fullscreen();

    CalibrationArea area(calibrator);
    win.add(area);
    area.show();

    Gtk::Main::run(win);

    Gtk::Main::quit();
    delete calibrator;
    return 0;
}
