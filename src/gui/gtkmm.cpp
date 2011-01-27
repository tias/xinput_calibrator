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
#include <gtkmm/main.h>
#include <gtkmm/window.h>
#include <gtkmm/drawingarea.h>
#include <cairomm/context.h>

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


/*******************************************
 * GTK-mm class for the the calibration GUI
 *******************************************/
class CalibrationArea : public Gtk::DrawingArea
{
public:
    CalibrationArea(Calibrator* w);

protected:
    // Data
    Calibrator* calibrator;
    double X[4], Y[4];
    int display_width, display_height;
    int time_elapsed;

    const char* message;

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

CalibrationArea::CalibrationArea(Calibrator* calibrator0)
  : calibrator(calibrator0), time_elapsed(0), message(NULL)
{
    // Listen for mouse events
    add_events(Gdk::KEY_PRESS_MASK | Gdk::BUTTON_PRESS_MASK);
    set_flags(Gtk::CAN_FOCUS);

    set_display_size(get_width(), get_height());

    // Setup timer for animation
    sigc::slot<bool> slot = sigc::mem_fun(*this, &CalibrationArea::on_timer_signal);
    Glib::signal_timeout().connect(slot, time_step);
}

void CalibrationArea::set_display_size(int width, int height) {
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

bool CalibrationArea::on_expose_event(GdkEventExpose *event)
{
    // check that screensize did not change
    if (display_width != get_width() ||
         display_height != get_height()) {
        set_display_size(get_width(), get_height());
    }

    Glib::RefPtr<Gdk::Window> window = get_window();
    if (window) {
        Cairo::RefPtr<Cairo::Context> cr = window->create_cairo_context();
        cr->save();

        cr->rectangle(event->area.x, event->area.y, event->area.width, event->area.height);
        cr->clip();

        // Print the text
        cr->set_font_size(font_size);
        double text_height = -1;
        double text_width = -1;
        Cairo::TextExtents extent;
        for (int i = 0; i != help_lines; i++) {
            cr->get_text_extents(help_text[i], extent);
            text_width = std::max(text_width, extent.width);
            text_height = std::max(text_height, extent.height);
        }
        text_height += 2;

        double x = (display_width - text_width) / 2;
        double y = (display_height - text_height) / 2 - 60;
        cr->set_line_width(2);
        cr->rectangle(x - 10, y - (help_lines*text_height) - 10,
                text_width + 20, (help_lines*text_height) + 20);

        // Print help lines
        y -= 3;
        for (int i = help_lines-1; i != -1; i--) {
            cr->get_text_extents(help_text[i], extent);
            cr->move_to(x + (text_width-extent.width)/2, y);
            cr->show_text(help_text[i]);
            y -= text_height;
        }
        cr->stroke();

        // Draw the points
        for (int i = 0; i <= calibrator->get_numclicks(); i++) {
            // set color: already clicked or not
            if (i < calibrator->get_numclicks())
                cr->set_source_rgb(1.0, 1.0, 1.0);
            else
                cr->set_source_rgb(0.8, 0.0, 0.0);

            cr->set_line_width(1);
            cr->move_to(X[i] - cross_lines, Y[i]);
            cr->rel_line_to(cross_lines*2, 0);
            cr->move_to(X[i], Y[i] - cross_lines);
            cr->rel_line_to(0, cross_lines*2);
            cr->stroke();

            cr->arc(X[i], Y[i], cross_circle, 0.0, 2.0 * M_PI);
            cr->stroke();
        }

        // Draw the clock background
        cr->arc(display_width/2, display_height/2, clock_radius/2, 0.0, 2.0 * M_PI);
        cr->set_source_rgb(0.5, 0.5, 0.5);
        cr->fill_preserve();
        cr->stroke();

        cr->set_line_width(clock_line_width);
        cr->arc(display_width/2, display_height/2, (clock_radius - clock_line_width)/2,
             3/2.0*M_PI, (3/2.0*M_PI) + ((double)time_elapsed/(double)max_time) * 2*M_PI);
        cr->set_source_rgb(0.0, 0.0, 0.0);
        cr->stroke();


        // Draw the message (if any)
        if (message != NULL) {
            // Frame the message
            cr->set_font_size(font_size);
            Cairo::TextExtents extent;
            cr->get_text_extents(this->message, extent);
            text_width = extent.width;
            text_height = extent.height;

            x = (display_width - text_width) / 2;
            y = (display_height - text_height + clock_radius) / 2 + 60;
            cr->set_line_width(2);
            cr->rectangle(x - 10, y - text_height - 10,
                    text_width + 20, text_height + 25);

            // Print the message
            cr->move_to(x, y);
            cr->show_text(this->message);
            cr->stroke();
        }

        cr->restore();
    }

    return true;
}

void CalibrationArea::redraw()
{
    Glib::RefPtr<Gdk::Window> win = get_window();
    if (win) {
        const Gdk::Rectangle rect(0, 0, display_width, display_height);
        win->invalidate_rect(rect, false);
    }
}

bool CalibrationArea::on_timer_signal()
{
    time_elapsed += time_step;
    if (time_elapsed > max_time) {
        exit(0);
    }

    // Update clock
    Glib::RefPtr<Gdk::Window> win = get_window();
    if (win) {
        const Gdk::Rectangle rect(display_width/2 - clock_radius - clock_line_width,
                                 display_height/2 - clock_radius - clock_line_width,
                                 2 * clock_radius + 1 + 2 * clock_line_width,
                                 2 * clock_radius + 1 + 2 * clock_line_width);
        win->invalidate_rect(rect, false);
    }

    return true;
}

bool CalibrationArea::on_button_press_event(GdkEventButton *event)
{
    // Handle click
    time_elapsed = 0;
    bool success = calibrator->add_click((int)event->x_root, (int)event->y_root);

    if (!success && calibrator->get_numclicks() == 0) {
        draw_message("Mis-click detected, restarting...");
    } else {
        draw_message(NULL);
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

void CalibrationArea::draw_message(const char* msg)
{
    this->message = msg;
}

bool CalibrationArea::on_key_press_event(GdkEventKey *event)
{
    (void) event;
    exit(0);
}
