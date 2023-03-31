#pragma once

#include "utils.h"

struct Options {
	// gfx
	bool vsync            = true;
	bool lazy_render      = true;	// render only when something has changed

	// log
	bool print_to_file    = true;
	bool print_to_console = true;
	bool quit_on_fatal    = true;

	void load(const char *filename = "options.ini");
	void update();
	void cleanup();

	static Options &get();

private:
	file::Watcher watcher = "./";
};