#include <algorithm>
#include <stdio.h>
#include <vector>

#include "calibrator.hh"
#include "calibrator/Tester.hpp"

int main() {
    // screen dimensions
    int width = 800;
    int height = 600;
    XYinfo screen_res(0, width, 0, height);

    int delta_x = width/(float)num_blocks;
    int delta_y = height/(float)num_blocks;
    XYinfo target(delta_x, width-delta_x, delta_y, height-delta_y);

    int slack = 2; // amount of pixels result can be off target

    XYinfo dev_res(0, 1000, 0, 1000);
    XYinfo old_axis(0, 1000, 0, 1000);

    // raw device coordinates to emulate
    std::vector<XYinfo> raw_coords;
    // normal
    raw_coords.push_back( XYinfo(105, 783, 233, 883) );
    // invert x, y, x+y
    raw_coords.push_back( XYinfo(783, 105, 233, 883) );
    raw_coords.push_back( XYinfo(105, 783, 883, 233) );
    raw_coords.push_back( XYinfo(783, 105, 883, 233) );
    // swap
    raw_coords.push_back( XYinfo(233, 883, 105, 783) );
    // swap and inverts
    raw_coords.push_back( XYinfo(233, 883, 783, 105) );
    raw_coords.push_back( XYinfo(883, 233, 105, 783) );
    raw_coords.push_back( XYinfo(883, 233, 783, 105) );

    CalibratorTester calib("Tester", old_axis);

    for (unsigned c=0; c != raw_coords.size(); c++) {
        XYinfo raw(raw_coords[c]);
        printf("Raw: "); raw.print();

        // clicked from raw
        XYinfo clicked = calib.emulate_driver(raw, false);//false=old_axis, screen_res, dev_res);
        printf("Clicked: "); clicked.print();

        // emulate screen clicks
        calib.add_click(clicked.x.min, clicked.y.min);
        calib.add_click(clicked.x.max, clicked.y.min);
        calib.add_click(clicked.x.min, clicked.y.max);
        calib.add_click(clicked.x.max, clicked.y.max);
        calib.finish(width, height);

        // test result
        XYinfo result = calib.emulate_driver(raw, true); // true=new_axis

        if (abs(target.x.min - result.x.min) > slack ||
            abs(target.x.max - result.x.max) > slack ||
            abs(target.y.min - result.y.min) > slack ||
            abs(target.y.max - result.y.max) > slack) {
            printf("Error: difference between target and result > %i:\n", slack);
            printf("\tTarget: "); target.print();
            printf("\tResult: "); result.print();
            exit(1);
        }
        printf("OK\n");

    } // loop over raw_coords
}
