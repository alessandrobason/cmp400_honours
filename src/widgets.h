#pragma once

#include "vec.h"
#include "d3d11_fwd.h"
#include "common.h"

enum class LogLevel;
struct Texture3D;
struct Shader;

struct BrushEditor;
struct MaterialEditor;
struct Texture2D;

template<typename T> struct Handle;

void fpsWidget();
void imageViewWidget(Handle<Texture2D> texture, vec4 *out_bounds = nullptr);
void mainViewWidget();
//void mainTargetWidget(Handle<Texture2D> texture);
void messagesWidget();
void addMessageToWidget(LogLevel severity, const char *message, float show_time = 3.f);

void keyRemapper();

void mainMenuBar(BrushEditor &be, MaterialEditor &me, Handle<Texture3D> main_tex);
void saveLoadFileDialog(Handle<Texture3D> main_texture, Handle<Shader> scale_cs);

bool filledSlider(const char *str_id, float *data, float vmin, float vmax, const char *fmt = "%.3f");
bool inputU8(const char *label, uint8_t *data, uint8_t step = 1, uint8_t step_fast = 10);

bool inputUint(const char *label, uint &value, uint step = 1, int flags = 0);
bool imInputUint2(const char *label, unsigned int v[2], int flags = 0);
bool imInputUint3(const char *label, unsigned int v[3], int flags = 0);
bool sliderUInt(const char *label, unsigned int *v, unsigned int vmin, unsigned int vmax, int flags = 0);
bool sliderUInt2(const char *label, unsigned int v[2], unsigned int vmin, unsigned int vmax, int flags = 0);
bool sliderUInt3(const char *label, unsigned int v[3], unsigned int vmin, unsigned int vmax, int flags = 0);
void separatorText(const char *label);

void tooltip(const char *msg, bool same_line = true);
