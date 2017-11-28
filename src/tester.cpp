#include <iostream>
#include <memory>
#include <vector>

#include "calibrator.hh"
#include "calibrator/Tester.hpp"
#include "calibrator/EvdevTester.hpp"

int main() {
    // screen dimensions
    constexpr int width = 800;
    constexpr int height = 600;
    constexpr XYinfo screen_res{0, width, 0, height};

    constexpr int delta_x = static_cast<int>(width/static_cast<float>(num_blocks));
    constexpr int delta_y = static_cast<int>(height/static_cast<float>(num_blocks));
    constexpr XYinfo target{delta_x, width-delta_x, delta_y, height-delta_y};

    constexpr int slack = 2; // amount of pixels result can be off target

    constexpr XYinfo dev_res{0, 1000, 0, 1000};

    std::vector<XYinfo> old_axes{
        {0, 1000, 0, 1000}, {1000, 0, 0, 1000}, {0, 1000, 1000, 0},
        {1000, 0, 0, 1000}, {0, 1000, 0, 1000, 1, 0, 0},
        {0, 1000, 0, 1000, 1, 0, 1}, {0, 1000, 0, 1000, 1, 1, 0},
        {0, 1000, 0, 1000, 1, 1, 1}, {1000, 0, 0, 1000, 1, 0, 0},
        {1000, 0, 0, 1000, 1, 0, 1}, {1000, 0, 0, 1000, 1, 1, 0},
        {1000, 0, 0, 1000, 1, 1, 1},
        // non device-resolution calibs
        {42, 929, 20, 888},
        // xf86ScaleAxis rounds to min/max, this can lead to inaccurate
        // results! Can we fix that?
        {42, 929, 20, 888}
        // {-9, 895, 124, 990}, // this is the true axis
                                // rounding error when raw_coords are swapped???
        // {75, 750, 20, 888}, // rounding error on X axis
        // {42, 929, 120, 888} // rounding error on Y axis
    };

    // raw device coordinates to emulate
    std::vector<XYinfo> raw_coords{
        // normal
        {105, 783, 233, 883},
        // invert x, y, x+y
        {783, 105, 233, 883},
        {105, 783, 883, 233},
        {783, 105, 883, 233},
        // swap
        {233, 883, 105, 783},
        // swap and inverts
        {233, 883, 783, 105},
        {883, 233, 105, 783},
        {883, 233, 783, 105}
    };

    std::unique_ptr<CalibratorTesterInterface> calib;
    for (unsigned t=0; t<=1; t++) {
        if (t == 0)
            std::cout << "CalibratorTester\n";
        else if (t == 1)
            std::cout << "CalibratorEvdevTester\n";

    for (const XYinfo &oldAxis: old_axes) {
        XYinfo old_axis{oldAxis};
        std::cout << "Old axis: ";
        old_axis.print();

    for (const XYinfo &rawCoords: raw_coords) {
        const XYinfo raw{rawCoords};
        //printf("Raw: "); raw.print();

        // reset calibrator object
        if (t == 0)
            calib.reset(new CalibratorTester{"Tester", old_axis});
        else if (t == 1)
            calib.reset(new CalibratorEvdevTester{"Tester", old_axis});

        // clicked from raw
        const XYinfo clicked{calib->emulate_driver(raw, false, screen_res, dev_res)};// false=old_axis
        //printf("\tClicked: "); clicked.print();

        // emulate screen clicks
        calib->add_click(clicked.x.min, clicked.y.min);
        calib->add_click(clicked.x.max, clicked.y.min);
        calib->add_click(clicked.x.min, clicked.y.max);
        calib->add_click(clicked.x.max, clicked.y.max);
        calib->finish(width, height);

        // test result
        const XYinfo result{calib->emulate_driver(raw, true, screen_res, dev_res)}; // true=new_axis

        const int maxdiff = std::max(abs(target.x.min - result.x.min),
                            std::max(abs(target.x.max - result.x.max),
                            std::max(abs(target.y.min - result.y.min),
                                     abs(target.y.max - result.y.max)))); // no n-ary max in c++??
        if (maxdiff > slack) {
            std::cout << "-\n";
            std::cout << "Old axis: "; old_axis.print();
            std::cout << "Raw: "; raw.print();
            std::cout << "Clicked: "; clicked.print();
            std::cout << "New axis: "; calib->new_axis_print();
            std::cout << "Error: difference between target and result: "
                      << maxdiff << " > " << slack << ":\n";
            std::cout << "\tTarget: "; target.print();
            std::cout << "\tResult: "; result.print();
            exit(1);
        }

        std::cout << maxdiff;
    } // loop over raw_coords

        std::cout << ". OK\n";
    } // loop over old_axes

        std::cout << "\n";
    } // loop over calibrators
}
