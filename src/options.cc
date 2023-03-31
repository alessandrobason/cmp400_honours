#include "options.h"

#include <assert.h>
#include "ini.h"
#include "tracelog.h"

void Options::load(const char *filename) {
	watcher.watchFile(filename);

	ini::Doc doc(filename);
	
	if (auto gfx = doc.get("gfx")) {
		gfx->get("vsync").trySet(vsync);
		gfx->get("lazy render").trySet(lazy_render);
	}

	if (auto log = doc.get("log")) {
		log->get("print to file").trySet(print_to_file);
		log->get("print to console").trySet(print_to_console);
		log->get("quit on fatal").trySet(quit_on_fatal);
	}
}

void Options::update() {
	watcher.update();

	auto file = watcher.getChangedFiles();
	while (file) {
		load(file->name.get());
		info("reloading options");
		file = watcher.getChangedFiles(file);
		assert(!file);
	}
}

void Options::cleanup() {
	watcher.cleanup();
}

Options &Options::get() {
	static Options g_options;
	return g_options;
}
