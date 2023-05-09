#include "options.h"

#include <assert.h>
#include <imgui.h>

#include "ini.h"
#include "tracelog.h"
#include "widgets.h"
#include "system.h"
#include "texture.h"

void Options::load(const char *filename) {
	watcher.watchFile(filename);

	ini::Doc doc(filename);
	
	if (auto gfx = doc.get("gfx")) {
		gfx->get("vsync").trySet(vsync);
		gfx->get("auto capture").trySet(auto_capture);
		gfx->get("show fps").trySet(show_fps);
		gfx->get("autosave").trySet(auto_save_mins);
		if (ini::Value res = gfx->get("resolution")) {
			arr<str::view> vec = res.asVec();
			if (vec.size() == 2) {
				resolution.x = str::toUInt(vec[0].data);
				resolution.y = str::toUInt(vec[1].data);
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

void Options::save(const char *filename) {
	fs::file fp(filename, "wb");

#define B(b) (b) ? "true" : "false"

	fp.puts("[gfx]\n");
	fp.print("vsync = %s\n", B(vsync));
	fp.print("resolution = %u %u\n", resolution.x, resolution.y);
	fp.print("auto capture = %s\n", B(auto_capture));
	fp.print("show fps = %s\n", B(show_fps));
	fp.print("autosave = %.2f\n", auto_save_mins);

	fp.puts("\n[camera]\n");
	fp.print("zoom sensitivity = %.3f\n", zoom_sensitivity);
	fp.print("look sensitivity = %.3f\n", look_sensitivity);

	fp.puts("\n[log]\n");
	fp.print("print to file = %s\n", B(print_to_file));
	fp.print("print to console = %s\n", B(print_to_console));
	fp.print("quit on fatal = %s\n", B(quit_on_fatal));

#undef B
}

void Options::drawWidget() {
	if (!is_open) return;
	if (!ImGui::Begin("Options", &is_open)) {
		ImGui::End();
		return;
	}

	if (ImGui::Button("Save")) {
		save("options.ini");
	}

	separatorText("Graphics");
	ImGui::Checkbox("VSync enabled", &vsync);
	tooltip("When VSync is enabled, the application caps its maximum number of frames per seconds, otherwise it tries to run as fast as possible");
	
	ImGui::PushItemWidth(-1);
		ImGui::Text("Image resolution");
		if (inputUint2("##resolution", resolution.data)) {
			if (all(resolution != 0)) {
				gfx::main_rtv->resize(resolution.x, resolution.y);
			}
		}
	ImGui::PopItemWidth();

	ImGui::Checkbox("Auto Capture", &auto_capture);
	tooltip("(Debugging only) Captures the frame every time a sculpting operation happens, only useful if the application is being debugged with RenderDoc");
	ImGui::Checkbox("Show FPS", &show_fps);

	ImGui::DragFloat("Auto Save", &auto_save_mins, 0.1f, 0.f, 10.f, "%.3f minute(s)");
	tooltip("How many minutes before the sculpture auto saves, keep in mind that you need to save it at least once first!");

	separatorText("Camera");
	ImGui::SliderFloat("Zoom sensitivity", &zoom_sensitivity, 1, 50);
	ImGui::SliderFloat("Look sensitivity", &look_sensitivity, 1, 300);

	separatorText("Logger");
	ImGui::Checkbox("Print to file", &print_to_file);
	ImGui::Checkbox("Print to console", &print_to_console);
	ImGui::Checkbox("Quit on fatal", &quit_on_fatal);

	ImGui::End();
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

void Options::setOpen(bool new_is_open) {
	is_open = new_is_open;
}

bool Options::isOpen() const {
	return is_open;
}

Options &Options::get() {
	static Options g_options;
	return g_options;
}