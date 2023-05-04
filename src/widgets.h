#pragma once

#include "vec.h"
#include "d3d11_fwd.h"
#include "common.h"

enum class LogLevel;
struct Texture3D;
struct Shader;

struct BrushEditor;
struct MaterialEditor;
struct RayTracingEditor;
struct Sculpture;
struct Texture2D;

template<typename T> struct Handle;

typedef int ImGuiSliderFlags;

namespace widgets {
	void setupMenuBar(BrushEditor &be, MaterialEditor &me, RayTracingEditor &re, Sculpture &sc);

	void fps();
	void imageView(Handle<Texture2D> texture, vec4 *out_bounds = nullptr);
	void mainView();
	void messages();
	void addMessage(LogLevel severity, const char *message, float show_time = 3.f);
	void keyRemapper();
	void controlsPage();
	void menuBar();
	void saveLoadFile();
} // namespace widgets

// ImGui Extended

bool filledSlider(const char *str_id, float *data, float vmin, float vmax, const char *fmt = "%.3f", ImGuiSliderFlags flags = 0);
bool inputU8(const char *label, uint8_t *data, uint8_t step = 1, uint8_t step_fast = 10);

bool inputUint(const char *label, uint &value, uint step = 1, int flags = 0);
bool inputUint2(const char *label, unsigned int v[2], int flags = 0);
bool inputUint3(const char *label, unsigned int v[3], int flags = 0);
bool sliderUInt(const char *label, unsigned int *v, unsigned int vmin, unsigned int vmax, int flags = 0);
bool sliderUInt2(const char *label, unsigned int v[2], unsigned int vmin, unsigned int vmax, int flags = 0);
bool sliderUInt3(const char *label, unsigned int v[3], unsigned int vmin, unsigned int vmax, int flags = 0);
void separatorText(const char *label);
void tooltip(const char *msg, bool same_line = true);
