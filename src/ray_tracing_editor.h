#pragma once

#include "common.h"
#include "vec.h"
#include "timer.h"
#include "handle.h"

struct DynamicShader;
struct MaterialEditor;
struct Shader;
struct Texture2D;
struct Texture3D;
struct Buffer;

struct RayTracingEditor {
	struct RayTraceData {
		vec2u thread_loc         = 0;
		uint num_rendered_frames = 0;
		uint num_of_lights       = 0;
		uint maximum_steps       = 500;
		uint maximum_bounces     = 10;
		uint maximum_rays        = 10;
		float maximum_trace_dist = 3000.f;
	};

	RayTracingEditor(DynamicShader &ds);

	bool update(const MaterialEditor &me);
	void widget();
	void reset();
	void resize(const vec2i &size);
	void step(MaterialEditor &me, Handle<Texture3D> main_tex, Handle<Buffer> shader_data);

	bool isRendering() const;
	bool shouldRedraw(bool is_dirty) const;
	Handle<Texture2D> getImage() const;

private:
	void imageWidget();
	void timeWidget();
	void editorWidget();

	Handle<Shader> shader;
	Handle<Texture2D> image;
	Handle<Buffer> data_handle;
	RayTraceData data;
	//vec2u thread_loc = 0;
	//uint rendered_frames = 0;
	uint64_t start_render = 0;
	bool is_rendering = false;
	bool is_view_open = true;
	bool is_view_active = false;
	bool is_editor_open = true;
	bool should_redraw = false;
	bool refresh = true;
	CPUClock rough_clock = "ray tracing frame";
};