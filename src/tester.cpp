#include <algorithm>
#include <stdio.h>
#include <vector>

#include "calibrator.hh"
#include "calibrator/Tester.hpp"
#include "calibrator/EvdevTester.hpp"

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

    std::vector<XYinfo> old_axes;
    old_axes.push_back( XYinfo(0, 1000, 0, 1000) );
    old_axes.push_back( XYinfo(1000, 0, 0, 1000) );
    old_axes.push_back( XYinfo(0, 1000, 1000, 0) );
    old_axes.push_back( XYinfo(1000, 0, 0, 1000) );
    old_axes.push_back( XYinfo(0, 1000, 0, 1000, 1, 0, 0) );
    old_axes.push_back( XYinfo(0, 1000, 0, 1000, 1, 0, 1) );
    old_axes.push_back( XYinfo(0, 1000, 0, 1000, 1, 1, 0) );
    old_axes.push_back( XYinfo(0, 1000, 0, 1000, 1, 1, 1) );
    old_axes.push_back( XYinfo(1000, 0, 0, 1000, 1, 0, 0) );
    old_axes.push_back( XYinfo(1000, 0, 0, 1000, 1, 0, 1) );
    old_axes.push_back( XYinfo(1000, 0, 0, 1000, 1, 1, 0) );
    old_axes.push_back( XYinfo(1000, 0, 0, 1000, 1, 1, 1) );
    // non device-resolution calibs
    old_axes.push_back( XYinfo(42, 929, 20, 888) );
    // xf86ScaleAxis rounds to min/max, this can lead to inaccurate
    // results! Can we fix that?
    old_axes.push_back( XYinfo(42, 929, 20, 888) );
    //old_axes.push_back( XYinfo(-9, 895, 124, 990) ); // this is the true axis
                                                       // rounding error when raw_coords are swapped???
    //old_axes.push_back( XYinfo(75, 750, 20, 888) ); // rounding error on X axis
    //old_axes.push_back( XYinfo(42, 929, 120, 888) ); // rounding error on Y axis

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

    CalibratorTesterInterface* calib;
    for (unsigned t=0; t<=1; t++) {
        if (t == 0)
            printf("CalibratorTester\n");
        else if (t == 1)
            printf("CalibratorEvdevTester\n");

    for (unsigned a=0; a != old_axes.size(); a++) {
        XYinfo old_axis(old_axes[a]);
        printf("Old axis: "); old_axis.print();

    for (unsigned c=0; c != raw_coords.size(); c++) {
        XYinfo raw(raw_coords[c]);
        //printf("Raw: "); raw.print();

        // reset calibrator object
        if (t == 0)
            calib = new CalibratorTester("Tester", old_axis);
        else if (t == 1)
            calib = new CalibratorEvdevTester("Tester", old_axis);

        // clicked from raw
        XYinfo clicked = calib->emulate_driver(raw, false, screen_res, dev_res);// false=old_axis
        //printf("\tClicked: "); clicked.print();

        // emulate screen clicks
        calib->add_click(clicked.x.min, clicked.y.min);
        calib->add_click(clicked.x.max, clicked.y.min);
        calib->add_click(clicked.x.min, clicked.y.max);
        calib->add_click(clicked.x.max, clicked.y.max);
        calib->finish(width, height);

        // test result
        XYinfo result = calib->emulate_driver(raw, true, screen_res, dev_res); // true=new_axis

        int maxdiff = std::max(abs(target.x.min - result.x.min),
                      std::max(abs(target.x.max - result.x.max),
                      std::max(abs(target.y.min - result.y.min),
                               abs(target.y.max - result.y.max)))); // no n-ary max in c++??
        if (maxdiff > slack) {
            printf("-\n");
            printf("Old axis: "); old_axis.print();
            printf("Raw: "); raw.print();
            printf("Clicked: "); clicked.print();
            printf("New axis: "); calib->new_axis_print();
            printf("Error: difference between target and result: %i > %i:\n", maxdiff, slack);
            printf("\tTarget: "); target.print();
            printf("\tResult: "); result.print();
            exit(1);
        }

        printf("%i", maxdiff);
    } // loop over raw_coords

        printf(". OK\n");
    } // loop over old_axes

        printf("\n");
    } // loop over calibrators
}
