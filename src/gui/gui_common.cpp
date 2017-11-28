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

#include "gui/gui_common.hpp"


void get_display_texts(std::list<std::string> *texts, Calibrator *calibrator)
{
    std::string str;
    /* 1st line */
    str = "Touchscreen Calibration";
    const char* sysfs_name = calibrator->get_sysfs_name();
    if(sysfs_name != nullptr) {
        str += " for '";
        str += sysfs_name;
        str += "'";
	}
	texts->push_back(str);
    /* 2nd line */
    str = "Press the point, use a stylus to increase precision.";
	texts->push_back(str);
    /* 3rd line */
    str = "";
	texts->push_back(str);
    /* 4th line */
    str = "(To abort, press any key";
    if(calibrator->get_use_timeout())
        str += " or wait)";
    else
        str += ")";
	texts->push_back(str);
}
