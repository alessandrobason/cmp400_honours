#pragma once

#include "utils.h"
#include "vec.h"

struct Options {
	// gfx
	bool vsync            = true;
	bool lazy_render      = true;	// render only when something has changed
	vec2u resolution = vec2u(1920, 1080);

	// camera
	float zoom_sensitivity = 20.f;
	float look_sensitivity = 100.f;

	// log
	bool print_to_file    = true;
	bool print_to_console = true;
	bool quit_on_fatal    = true;

	void load(const char *filename = "options.ini");
	bool update();
	void cleanup();

	static Options &get();

private:
	file::Watcher watcher = "./";
};