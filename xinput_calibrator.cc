/*
 * Copyright © 2008 Soren Hauberg
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of Red Hat
 * not be used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.  Red
 * Hat makes no representations about the suitability of this software
 * for any purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * THE AUTHORS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors:
 *	Soren Hauberg (haub...@gmail.com)
 */

/*
 * This program performs calibration of a touchscreen that uses the 'usbtouchscreen'
 * Linux kernel module. A calibration consists of finding the following parameters:
 *
 *   'flip_x'
 *      a boolean parameter that determines if the x-coordinate should be flipped.
 *
 *   'flip_y'
 *      same as 'flip_x' except for the y-coordinate.
 *
 *   'swap_xy'
 *      a boolean parameter that determines if the x and y-coordinates should
 *      be swapped.
 *
 *   'min_x', 'max_x', 'min_y', and 'max_y'
 *      the minimum and maximum x and y values that the screen can report. In principle
 *      we could get these parameters by letting the user press the corners of the
 *      screen. In practice this doesn't work, because the screen doesn't always have
 *      sensors at the corners. So, what we do is we ask the user to press points that
 *      have been pushed a bit closer to the center, and then we extrapolate the
 *      parameters.
 */

#include <cstring>

#include <gtkmm/main.h>
#include <gtkmm/window.h>
#include <gtkmm/drawingarea.h>
#include <cairomm/context.h>

/* Number of points the user should click */
const int num_points = 4;

/* Number of blocks. We partition we screen into 'num_blocks' x 'num_blocks'
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

/* Names of the points */
enum {
  UL = 0, /* Upper-left */
  UR = 1, /* Upper-right */
  LL = 2, /* Lower-left */
  LR = 3  /* Lower-right */
};

/* Output ranges. The final output will be scaled to [0, range_x] x [0, range_y] */
const int range_x = 1023;
const int range_y = 1023;

/* The file to which the calibration parameters are saved. (XXX: is this distribution dependend?) */
const char *modprobe_conf_local = "/etc/modprobe.conf.local";

/* Prefix to the kernel path where we can set the parameters */
const char *module_prefix = "/sys/module/usbtouchscreen/parameters";

/* Names of kernel parameters */
const char *p_range_x = "range_x";
const char *p_range_y = "range_y";
const char *p_min_x = "min_x";
const char *p_min_y = "min_y";
const char *p_max_x = "max_x";
const char *p_max_y = "max_y";
const char *p_transform_xy = "transform_xy";
const char *p_flip_x = "flip_x";
const char *p_flip_y = "flip_y";
const char *p_swap_xy = "swap_xy";

/* Threshold to keep the same point from being clicked twice. Set to zero if you don't
 * want this check */
const int click_threshold = 7;

/* Timeout parameters */
const int time_step = 100; /* 500 = 0.1 sec */
const int max_time = 5000; /* 5000 = 5 sec */

/* Clock Appereance */
const int clock_radius = 25;
const int clock_line_width = 10;

/* Globals for kernel parameters from startup. We revert to these if the program aborts */
bool val_transform_xy, val_flip_x, val_flip_y, val_swap_xy;

/* Text printed on screen */
const int font_size = 20;
const std::string help_text = "Press the point with the stylus";

/* Helper functions */
char yesno (const bool value)
{
  if (value)
    return 'Y';
  else
    return 'N';
}

void read_int_parameter (const char *param, int &value)
{
  char filename [100];
  sprintf (filename, "%s/%s", module_prefix, param);
  FILE *fid = fopen (filename, "r");
  if (fid == NULL)
    {
      fprintf (stderr, "Could not read parameter '%s'\n", param);
      return;
    }

  fscanf (fid, "%d", &value);
  fclose (fid);
}

void read_bool_parameter (const char *param, bool &value)
{
  char filename [100];
  sprintf (filename, "%s/%s", module_prefix, param);
  FILE *fid = fopen (filename, "r");
  if (fid == NULL)
    {
      fprintf (stderr, "Could not read parameter '%s'\n", param);
      return;
    }

  char val [3];
  fgets (val, 2, fid);
  fclose (fid);

  value = (val [0] == yesno (true));
}

void write_int_parameter (const char *param, const int value)
{
  char filename [100];
  sprintf (filename, "%s/%s", module_prefix, param);
  FILE *fid = fopen (filename, "w");
  if (fid == NULL)
    {
      fprintf (stderr, "Could not save parameter '%s'\n", param);
      return;
    }

  fprintf (fid, "%d", value);
  fclose (fid);
}

void write_bool_parameter (const char *param, const bool value)
{
  char filename [100];
  sprintf (filename, "%s/%s", module_prefix, param);
  FILE *fid = fopen (filename, "w");
  if (fid == NULL)
    {
      fprintf (stderr, "Could not save parameter '%s'\n", param);
      return;
    }

  fprintf (fid, "%c", yesno (value));
  fclose (fid);
}

void quit (int status)
{
  if (status != 0)
    {
      // Dirty exit, so we restore the parameters of the running kernel
      write_bool_parameter (p_transform_xy, val_transform_xy);
      write_bool_parameter (p_flip_x, val_flip_x);
      write_bool_parameter (p_flip_y, val_flip_y);
      write_bool_parameter (p_swap_xy, val_swap_xy);
    }

  Gtk::Main::quit ();
}

/* Class for writing output parameters to running kernel and to modprobe,conf */
class CalibrationWriter
{
public:
  CalibrationWriter ();

  bool add_click (double cx, double cy);
  void finish ();
  void set_width (int w);
  void set_height (int h);

protected:
  double clicked_x [num_points], clicked_y [num_points];
  int num_clicks, width, height;
};

CalibrationWriter::CalibrationWriter ()
  : num_clicks (0)
{
}

void CalibrationWriter::set_width (int w)
{
  width = w;
}

void CalibrationWriter::set_height (int h)
{
  height = h;
}

bool CalibrationWriter::add_click (double cx, double cy)
{
  /* Check that we don't click the same point twice */
  if (num_clicks > 0 && click_threshold > 0
  && abs (cx - clicked_x [num_clicks-1]) < click_threshold
  && abs (cy - clicked_y [num_clicks-1]) < click_threshold)
    return false;

  clicked_x [num_clicks] = cx;
  clicked_y [num_clicks] = cy;
  num_clicks ++;

  return true;
}

void CalibrationWriter::finish ()
{
  /* Should x and y be swapped? */
  const bool swap_xy = (abs (clicked_x [UL] - clicked_x [UR]) < abs (clicked_y [UL] - clicked_y [UR]));
  if (swap_xy)
    {
      std::swap (clicked_x [LL], clicked_x [UR]);
      std::swap (clicked_y [LL], clicked_y [UR]);
    }

  /* Compute min/max parameters. These are scaled to [0, range_x] x [0, range_y] */
  int min_x = (range_x * (clicked_x [UL] + clicked_x [LL])) / (2 * width);
  int max_x = (range_x * (clicked_x [UR] + clicked_x [LR])) / (2 * width);
  int min_y = (range_y * (clicked_y [UL] + clicked_y [UR])) / (2 * height);
  int max_y = (range_y * (clicked_y [LL] + clicked_y [LR])) / (2 * height);

  /* Should x and y be flipped? */
  const bool flip_x = (min_x > max_x);
  const bool flip_y = (min_y > max_y);

  /* Add / subtract the ofset that comes from not having the points in the corners */
  const int delta_x = (max_x - min_x) / (num_blocks - 2);
  min_x -= delta_x;
  max_x += delta_x;
  const int delta_y = (max_y - min_y) / (num_blocks - 2);
  min_y -= delta_y;
  max_y += delta_y;

  /* If x and y has to be swapped we also have to swap the parameters */
  if (swap_xy)
    {
      std::swap (min_x, max_y);
      std::swap (min_y, max_x);
    }

  /* Send the estimated parameters to the currently running kernel */
  write_int_parameter (p_range_x, range_x);
  write_int_parameter (p_range_y, range_y);
  write_int_parameter (p_min_x, min_x);
  write_int_parameter (p_min_y, min_y);
  write_int_parameter (p_max_x, max_x);
  write_int_parameter (p_max_y, max_y);
  write_bool_parameter (p_transform_xy, true);
  write_bool_parameter (p_flip_x, flip_x);
  write_bool_parameter (p_flip_y, flip_y);
  write_bool_parameter (p_swap_xy, swap_xy);

  /* Write calibration parameters to modprobe_conf_local to keep the for the next boot */
  FILE *fid = fopen (modprobe_conf_local, "r");
  if (fid == NULL)
    {
      fprintf (stderr, "Can't open '%s' for reading. Make sure you have the necesary rights\n",
               modprobe_conf_local);
      quit (1);
    }
  std::string new_contents;
  const int len = 1024; // XXX: we currently don't handle lines that are longer than this
  char line [len];
  const char *opt = "options usbtouchscreen";
  const int opt_len = strlen (opt);
  while (fgets (line, len, fid))
    {
      if (strncmp (line, opt, opt_len) == 0) // This is the line we want to remove
        continue;
      new_contents += line;
    }
  fclose (fid);
  char new_opt [opt_len];
  sprintf (new_opt, "%s %s=%d %s=%d %s=%d %s=%d %s=%d %s=%d %s=%c %s=%c %s=%c %s=%c\n",
           opt, p_range_x, range_x, p_range_y, range_y, p_min_x, min_x, p_min_y, min_y,
           p_max_x, max_x, p_max_y, max_y, p_transform_xy, yesno (true),
           p_flip_x, yesno (flip_x), p_flip_y, yesno (flip_y), p_swap_xy, yesno (swap_xy));
  new_contents += new_opt;

  fid = fopen (modprobe_conf_local, "w");
  if (fid == NULL)
    {
      fprintf (stderr, "Can't open '%s' for writing. Make sure you have the necesary rights\n",
               modprobe_conf_local);
      quit (1);
    }
  fprintf (fid, "%s", new_contents.c_str ());
  fclose (fid);
}

/* Class for creating the calibration GUI */
class CalibrationArea : public Gtk::DrawingArea
{
public:
  CalibrationArea ();

protected:
  /* Helper functions */
  void redraw ();

  /* Signal handlers */
  bool on_timeout ();
  bool on_expose_event (GdkEventExpose *event);
  bool on_button_press_event (GdkEventButton *event);

  /* Data */
  double X [num_points], Y [num_points];
  int num_clicks, width, height;
  CalibrationWriter W;
  int time_elapsed;
};

CalibrationArea::CalibrationArea ()
  : num_clicks (0), time_elapsed (0)
{
  /* Listen for mouse events */
  add_events (Gdk::BUTTON_PRESS_MASK);

  /* Compute absolute circle centers */
  const Glib::RefPtr<Gdk::Screen> S = get_screen ();
  width = S->get_width ();
  height = S->get_height ();
  W.set_width (width);
  W.set_height (height);

  const int delta_x = width/num_blocks;
  const int delta_y = height/num_blocks;
  X [UL] = delta_x;             Y [UL] = delta_y;
  X [UR] = width - delta_x - 1; Y [UR] = delta_y;
  X [LL] = delta_x;             Y [LL] = height - delta_y - 1;
  X [LR] = width - delta_x - 1; Y [LR] = height - delta_y - 1;

  /* Reset the currently running kernel */
  read_bool_parameter (p_transform_xy, val_transform_xy);
  read_bool_parameter (p_flip_x, val_flip_x);
  read_bool_parameter (p_flip_y, val_flip_y);
  read_bool_parameter (p_swap_xy, val_swap_xy);

  write_bool_parameter (p_transform_xy, false);
  write_bool_parameter (p_flip_x, false);
  write_bool_parameter (p_flip_y, false);
  write_bool_parameter (p_swap_xy, false);

  /* Setup timer for animation */
  sigc::slot<bool> slot = sigc::mem_fun (*this, &CalibrationArea::on_timeout);
  Glib::signal_timeout().connect (slot, time_step);
}

bool CalibrationArea::on_button_press_event (GdkEventButton *event)
{
  /* Handle click */
  time_elapsed = 0;
  const bool accepted = W.add_click (event->x_root, event->y_root);
  if (accepted)
    num_clicks ++;

  /* Are we done yet? */
  if (num_clicks >= num_points)
    {
      /* Save data to disk */
      W.finish ();

      /* Quit */
      quit (0);
    }

  /* Force a redraw */
  redraw ();

  return true;
}

void CalibrationArea::redraw ()
{
  Glib::RefPtr<Gdk::Window> win = get_window ();
  if (win)
    {
      const Gdk::Rectangle rect (0, 0, width, height);
      win->invalidate_rect (rect, false);
    }
}

bool CalibrationArea::on_expose_event (GdkEventExpose *event)
{
  const double radius = 4;
  Glib::RefPtr<Gdk::Window> window = get_window ();
  if (window)
    {
      Cairo::RefPtr<Cairo::Context> cr = window->create_cairo_context ();
      cr->save ();

      cr->rectangle (event->area.x, event->area.y, event->area.width, event->area.height);
      cr->clip ();

      /* Print the text */
      Cairo::TextExtents extent;
      cr->set_font_size (font_size);
      cr->get_text_extents (help_text, extent);
      cr->move_to ((width-extent.width)/2, 0.3*height);
      cr->show_text (help_text);
      cr->stroke ();

      /* Draw the points */
      for (int i = 0; i < num_clicks; i++)
        {
          cr->arc (X [i], Y [i], radius, 0.0, 2.0 * M_PI);
          cr->set_source_rgb (1.0, 1.0, 1.0);
          cr->fill_preserve ();
          cr->stroke ();
        }

      cr->set_line_width (2);
      cr->arc (X [num_clicks], Y [num_clicks], radius, 0.0, 2.0 * M_PI);
      cr->set_source_rgb (0.8, 0.0, 0.0);
      //cr->fill_preserve ();
      cr->stroke ();

      /* Draw the clock */
      cr->arc (width/2, height/2, clock_radius, 0.0, 2.0 * M_PI);
      cr->set_source_rgb (0.5, 0.5, 0.5);
      cr->fill_preserve ();
      cr->stroke ();

      cr->set_line_width (clock_line_width);
      cr->arc (width/2, height/2, clock_radius - clock_line_width/2, 0.0,
               2.0 * M_PI * (double)time_elapsed/(double)max_time);
      cr->set_source_rgb (0.0, 0.0, 0.0);
      cr->stroke ();

      cr->restore ();
    }

  return true;
}

bool CalibrationArea::on_timeout ()
{
  time_elapsed += time_step;
  if (time_elapsed > max_time)
    quit (1);

  // Update clock
  Glib::RefPtr<Gdk::Window> win = get_window ();
  if (win)
    {
      const Gdk::Rectangle rect (width/2 - clock_radius - clock_line_width,
                                 height/2 - clock_radius - clock_line_width,
                                 2 * clock_radius + 1 + 2 * clock_line_width,
                                 2 * clock_radius + 1 + 2 * clock_line_width);
      win->invalidate_rect (rect, false);
    }

  return true;
}

int main(int argc, char** argv)
{
  Gtk::Main kit(argc, argv);

  Gtk::Window win;
  win.fullscreen ();

  CalibrationArea area;
  win.add (area);
  area.show ();

  Gtk::Main::run (win);

  return 0;
}
