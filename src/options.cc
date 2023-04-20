#include "options.h"

#include <assert.h>
#include "ini.h"
#include "tracelog.h"

void Options::load(const char *filename) {
	watcher.watchFile(filename);

	ini::Doc doc(filename);
	
	if (auto gfx = doc.get("gfx")) {
		gfx->get("vsync").trySet(vsync);
		if (ini::Value res = gfx->get("resolution")) {
			arr<std::string_view> vec = res.asVec();
			if (vec.size() == 2) {
				resolution.x = str::toUInt(vec[0].data());
				resolution.y = str::toUInt(vec[1].data());
			}
		}
	}

	if (auto camera = doc.get("camera")) {
		camera->get("zoom").trySet(zoom_sensitivity);
		camera->get("look").trySet(look_sensitivity);
	}

	if (auto log = doc.get("log")) {
		log->get("print to file").trySet(print_to_file);
		log->get("print to console").trySet(print_to_console);
		log->get("quit on fatal").trySet(quit_on_fatal);
	}
}

bool Options::update() {
	watcher.update();

	auto file = watcher.getChangedFiles();
	if (!file) {
		return false;
	}

	while (file) {
		load(file->name.get());
		info("reloading options");
		file = watcher.getChangedFiles(file);
		assert(!file);
	}

	return true;
}

void Options::cleanup() {
	watcher.cleanup();
}

Options &Options::get() {
	static Options g_options;
	return g_options;
}
