all: xinput_calibrator.x11 xinput_calibrator.gtkmm

xinput_calibrator.x11: main_x11.cpp gui_x11.cpp
	g++ -Wall main_x11.cpp -lX11 -lXi -o xinput_calibrator.x11
	cp xinput_calibrator.x11 xinput_calibrator

xinput_calibrator.gtkmm: main_gtkmm.cpp gui_gtkmm.cpp
	g++ -Wall main_gtkmm.cpp `pkg-config --cflags --libs gtkmm-2.4` -o xinput_calibrator.gtkmm

clean:
	rm -f xinput_calibrator xinput_calibrator.x11 xinput_calibrator.gtkmm
