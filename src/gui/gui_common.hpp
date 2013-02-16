/*
 * Copyright (c) 2013 Andreas Müller
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

#ifndef GUI_GUI_COMMON_HPP
#define GUI_GUI_COMMON_HPP

#include "calibrator.hh"
#include <list>
#include <string>

// Timeout parameters
const int time_step = 100;  // in milliseconds
const int max_time = 15000; // in milliseconds, 5000 = 5 sec

// Clock appereance
const int cross_lines = 25;
const int cross_circle = 4;
const int clock_radius = 50;
const int clock_line_width = 10;

// Text printed on screen
const int font_size = 16;

void get_display_texts(std::list<std::string> *texts, Calibrator *calibrator);

#endif
