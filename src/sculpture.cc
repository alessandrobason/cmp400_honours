#include "sculpture.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "system.h"
#include "texture.h"
#include "shader.h"
#include "brush_editor.h"
#include "options.h"
#include "widgets.h"

constexpr vec3u texture_size = 512;
static_assert(all(texture_size % 8 == 0));

Sculpture::Sculpture(BrushEditor &be) : brush_editor(be) {
	texture = Texture3D::create(texture_size, Texture3D::Type::r16_snorm);
	scale   = Shader::compile("scale_cs.hlsl", ShaderType::Compute);
	sculpt  = Shader::compile("sculpt_cs.hlsl", ShaderType::Compute);

	if (!texture) gfx::errorExit("could not create main 3D texture");
	if (!scale)   gfx::errorExit("could not compile scale shader");
	if (!sculpt)  gfx::errorExit("could not compile sculpt shader");

	brush_editor.runFillShader(Shapes::Box, ShapeData(vec3(0), 150, 20, 150), texture);
}

Sculpture::~Sculpture() {
	if (save_state == SaveState::Unsaved) {
		mem::ptr<char[]> save_name = save_path ? str::dup(save_path.get()) : fs::findFirstAvailable(".", "unsaved_sculpture_%d.bin");
		name = fs::getNameAndExt(save_name.get());
		int result = MessageBox(
			nullptr,
			(str::tstr)str::format("You have unsaved changes, would you like to save before closing? Your file will be saved as %s", name.data),
			//(str::tstr)str::format("%s has unsaved changes, would you like to save?", name ? name.data : "Unsaved"),
			TEXT("Unsaved Changed"),
			MB_YESNO | MB_ICONWARNING
		);
		if (result == IDYES) {
			if (all(save_quality == 0)) save_quality = texture->size;
			save(save_quality, mem::move(save_name));
		}
	}

	if (save_state == SaveState::Saving) {
		save_promise.join();
	}
}

void Sculpture::update() {
	if (save_state == SaveState::Saving) {
		if (save_promise.isFinished()) {
			save_promise.reset();
			save_state = save_promise.value ? SaveState::Saved : SaveState::Unsaved;
			updateWindowName();
		}
	}

	if (!name.empty()) {
		if (save_clock.every(Options::get().auto_save_mins * 60.f)) {
			if (save_state != SaveState::Saved) {
				widgets::addMessage(LogLevel::Info, "Autosaving...");
				save(save_quality);
			}
		}
	}
}

void Sculpture::runSculpt() {
	if (save_state == SaveState::Saved) save_state = SaveState::Unsaved;
	if (Options::get().auto_capture)    gfx::captureFrame();
	
	sculpt->dispatch(
		texture->size / 8, 
		{ brush_editor.getOperHandle() },
		{ brush_editor.getBrushSRV(), brush_editor.getDataSRV() },
		{ texture->uav }
	);
}

void Sculpture::save(const vec3u &quality) {
	if (save_state == SaveState::Saved) {
		widgets::addMessage(LogLevel::Info, "Already saved sculpture");
		return;
	}

	if (save_state == SaveState::Saving) {
		widgets::addMessage(LogLevel::Error, "Can't save sculpture, still busy saving previous one");
		return;
	}

	updateWindowName();
	save_state = SaveState::Saving;
	save_quality = quality;
	Handle<Texture3D> out_text = Texture3D::create(quality, texture->getType());
	scale->dispatch(quality / 8, {}, { texture->srv }, { out_text->uav });
	out_text->save(save_path.get(), true, &save_promise);
	out_text->cleanup();
}

void Sculpture::save(const vec3u &quality, mem::ptr<char[]> &&path) {
	save_path = mem::move(path);
	name = fs::getNameAndExt(save_path.get());
	save(quality);
}

const char *Sculpture::getPath() const {
	return save_path.get();
}

const char *Sculpture::getName() const {
	return name.data ? name.data : "(no name)";
}

void Sculpture::updateWindowName() {
	if (save_state == SaveState::Saving) return;

	mem::ptr<char[]> base_name = win::getBaseWindowName().toAnsi();
	
	switch (save_state) {
	case Sculpture::SaveState::Unsaved:
		win::setWindowName(str::format("%s - %s*", base_name.get(), name.data));
		break;
	case Sculpture::SaveState::Saved:
		win::setWindowName(str::format("%s - %s", base_name.get(), name.data));
		break;
	}
	
}