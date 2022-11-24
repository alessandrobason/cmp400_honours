#include "options.h"

#include "ini.h"

void Options::load(const char *filename) {
	ini::Doc doc(filename);
	
	const ini::Table &root = doc.root();

	if (auto value = root.get("vsync")) {
		vsync = value->asBool();
	}
}