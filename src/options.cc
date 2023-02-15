#include "options.h"

#include <assert.h>
#include "ini.h"
#include "tracelog.h"



void Options::load(const char *filename) {
	watcher.watchFile(filename);

	ini::Doc doc(filename);
	
	const ini::Table &root = doc.root();

	root.get("vsync").trySet(vsync);
	root.get("lazy render").trySet(lazy_render);
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
