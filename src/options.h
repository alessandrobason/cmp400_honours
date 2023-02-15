#pragma once

#include "utils.h"

struct Options {
	bool vsync = true;
	bool lazy_render = true;        // render only when something has changed

	void load(const char *filename = "options.ini");
	void update();
	void cleanup();

	static Options &get();

private:
	file::Watcher watcher = "./";
};