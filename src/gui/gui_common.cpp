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

#include "config.h"

#include "gui/gui_common.hpp"

#include "gettext.h"

#define _(String) gettext(String)

void get_display_texts(std::list<std::string> *texts, Calibrator *calibrator)
{
    std::string str;

    setlocale(LC_ALL, "");

    /* 1st line */
    str = "Touchscreen Calibration";
    const char* sysfs_name = calibrator->get_sysfs_name();
    if (sysfs_name != NULL) {
        char *tmp = (char *)alloca(strlen(_("Touchscreen Calibration for '%s'")) + strlen(sysfs_name) + 16);
        sprintf(tmp, _("Touchscreen Calibration for '%s'"), sysfs_name);
        str = tmp;
    } else {
        str = _("Touchscreen Calibration");
    }
    texts->push_back(str);
    /* 2nd line */
    str = _("Press the point, use a stylus to increase precision.");
    texts->push_back(str);
    /* 3rd line */
    str = "";
    texts->push_back(str);
    /* 4th line */
    if(calibrator->get_use_timeout())
        str = _("(To abort, press any key or wait)");
    else
        str = _("(To abort, press any key)");
    texts->push_back(str);

    setlocale(LC_ALL, "C");
}
