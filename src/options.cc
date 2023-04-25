#include "options.h"

#include <assert.h>
#include <imgui.h>
#include "ini.h"
#include "tracelog.h"
#include "widgets.h"
#include "system.h"

void Options::load(const char *filename) {
	watcher.watchFile(filename);

	ini::Doc doc(filename);
	
	if (auto gfx = doc.get("gfx")) {
		gfx->get("vsync").trySet(vsync);
		gfx->get("auto capture").trySet(auto_capture);
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

static void tooltip(const char *msg, bool same_line = true) {
	if (same_line) ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
		ImGui::TextUnformatted(msg);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

void Options::drawWidget() {
	if (!is_open) return;
	if (!ImGui::Begin("Options", &is_open)) {
		ImGui::End();
		return;
	}

	if (ImGui::Button("Save to file")) {

	}

	separatorText("Graphics");
	ImGui::Checkbox("VSync enabled", &vsync);
	if (imInputUint2("Game view resolution", resolution.data)) {
		if (all(resolution != 0)) {
			gfx::main_rtv.create(resolution.x, resolution.y);
		}
	}
	ImGui::Checkbox("Auto Capture", &auto_capture);
	tooltip("(Debugging only) Captures the frame every time a sculpting operation happens, only useful if the application is being debugged with RenderDoc");

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