#pragma once

struct Options {
	bool vsync = true;

	void load(const char *filename = "options.ini");
};