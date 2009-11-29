all: xinput_calibrator

xinput_calibrator: xinput_calibrator.cc
	g++ -Wall xinput_calibrator.cc `pkg-config --cflags --libs gtkmm-2.4` -o xinput_calibrator
