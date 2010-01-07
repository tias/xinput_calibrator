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
#include <algorithm>
#include "calibrator.hh"

Calibrator::Calibrator(const char* const drivername0, const XYinfo& axys0)
  : drivername(drivername0), old_axys(axys0), num_clicks(0)
{
}

int Calibrator::get_numclicks()
{
    return num_clicks;
}

bool Calibrator::add_click(double x, double y)
{
    // Check that we don't click the same point twice
    if (num_clicks > 0 && click_threshold > 0
     && abs (x - clicked_x[num_clicks-1]) < click_threshold
     && abs (y - clicked_y[num_clicks-1]) < click_threshold)
        return false;

    clicked_x[num_clicks] = x;
    clicked_y[num_clicks] = y;
    num_clicks ++;

    return true;
}

void Calibrator::finish(int width, int height)
{
    // Should x and y be swapped?
    const bool swap_xy = (abs (clicked_x [UL] - clicked_x [UR]) < abs (clicked_y [UL] - clicked_y [UR]));
    if (swap_xy) {
        std::swap(clicked_x[LL], clicked_x[UR]);
        std::swap(clicked_y[LL], clicked_y[UR]);
    }

    // Compute min/max coordinates.
    XYinfo axys;
    // These are scaled using the values of old_axys
    const float scale_x = (old_axys.x_max - old_axys.x_min)/(float)width;
    axys.x_min = ((clicked_x[UL] + clicked_x[LL]) * scale_x/2) + old_axys.x_min;
    axys.x_max = ((clicked_x[UR] + clicked_x[LR]) * scale_x/2) + old_axys.x_min;
    const float scale_y = (old_axys.y_max - old_axys.y_min)/(float)height;
    axys.y_min = ((clicked_y[UL] + clicked_y[UR]) * scale_y/2) + old_axys.y_min;
    axys.y_max = ((clicked_y[LL] + clicked_y[LR]) * scale_y/2) + old_axys.y_min;

    // Add/subtract the offset that comes from not having the points in the
    // corners (using the same coordinate system they are currently in)
    const int delta_x = (axys.x_max - axys.x_min) / (float)(num_blocks - 2);
    axys.x_min -= delta_x;
    axys.x_max += delta_x;
    const int delta_y = (axys.y_max - axys.y_min) / (float)(num_blocks - 2);
    axys.y_min -= delta_y;
    axys.y_max += delta_y;


    // If x and y has to be swapped we also have to swap the parameters
    if (swap_xy) {
        std::swap(axys.x_min, axys.y_max);
        std::swap(axys.y_min, axys.x_max);
    }

    // finish the data, driver specific
    finish_data(axys, swap_xy);
}
