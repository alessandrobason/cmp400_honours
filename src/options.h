#pragma once

#include "fs.h"
#include "vec.h"

struct Options {
	// gfx
	bool vsync              = true;
	vec2u resolution        = vec2u(1920, 1080);
	bool auto_capture       = false;
	bool show_fps           = true;
	float auto_save_mins    = 1.f;

	// camera
	float zoom_sensitivity  = 20.f;
	float look_sensitivity  = 100.f;

	// log
	bool print_to_file      = true;
	bool print_to_console   = true;
	bool quit_on_fatal      = true;

	void load(const char *filename = "options.ini");
	void save(const char *filename);
	void drawWidget();
	bool update();
	void cleanup();
	void setOpen(bool is_open);
	bool isOpen() const;

	static Options &get();

private:
	// force it to be a singleton
	Options() = default;
	fs::Watcher watcher = "./";
	bool is_open = false;
};
