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

#ifndef GUI_GTKMM_HPP
#define GUI_GTKMM_HPP

#include <gtkmm/drawingarea.h>
#include "calibrator.hh"
#include <list>

/*******************************************
 * GTK-mm class for the the calibration GUI
 *******************************************/
class CalibrationArea : public Gtk::DrawingArea
{
public:
    CalibrationArea(Calibrator* w);

private:
    // Data
    Calibrator* calibrator;
    double X[4], Y[4];
    int display_width, display_height;
    int time_elapsed = 0;
    std::list<std::string> display_texts;

    const char* message = nullptr;

    // Signal handlers
    bool on_timer_signal();
    bool on_expose_event(GdkEventExpose *event);
    bool on_button_press_event(GdkEventButton *event);
    bool on_key_press_event(GdkEventKey *event);

    // Helper functions
    void set_display_size(int width, int height);
    void redraw();
    void draw_message(const char* msg);
};

#endif
