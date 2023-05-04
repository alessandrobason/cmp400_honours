#pragma once

#include "handle.h"
#include "utils.h"
#include "timer.h"
#include "vec.h"

struct BrushEditor;
struct Texture3D;
struct Shader;

struct Sculpture {
	Sculpture(BrushEditor &brush_editor);
	~Sculpture();
	void update();
	void runSculpt();
	void save(const vec3u &quality);
	void save(const vec3u &quality, mem::ptr<char[]> &&path);
	const char *getPath() const;
	const char *getName() const;

	Handle<Texture3D> texture;
	Handle<Shader> scale;
	Handle<Shader> sculpt;

private:
	void updateWindowName();

	enum class SaveState {
		Unsaved, Saving, Saved
	};

	mem::ptr<char[]> save_path;
	thr::Promise<bool> save_promise;
	str::view name;
	BrushEditor &brush_editor;
	SaveState save_state = SaveState::Unsaved;
	IntervalClock save_clock;
	vec3u save_quality = 0;
};