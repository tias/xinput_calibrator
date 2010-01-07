all: xinput_calibrator.old xinput_calibrator.gtkmm

xinput_calibrator.old: xinput_calibrator.cc
	g++ -Wall xinput_calibrator.cc `pkg-config --cflags --libs gtkmm-2.4` -o xinput_calibrator.old

xinput_calibrator.gtkmm: main_gtkmm.cpp gui_gtkmm.cpp
	g++ -Wall main_gtkmm.cpp `pkg-config --cflags --libs gtkmm-2.4` -o xinput_calibrator.gtkmm
