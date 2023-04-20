#include <stdio.h>
//#include <iostream>

#include "system.h"
#include "input.h"
#include "maths.h"
#include "options.h"
#include "tracelog.h"
#include "widgets.h"
#include "macros.h"
#include "timer.h"
#include "gfx.h"
#include "shape_builder.h"
#include "camera.h"
#include "matrix.h"
#include "brush.h"

#include <imgui.h>

#include <d3d11.h>
//#include <functional>

#include "utils.h"

//#include <tracy/TracyD3D11.hpp>

bool is_dirty = true;
constexpr vec3u maintex_size = 512;
constexpr vec3u brush_size = 64;
constexpr vec3u threads = maintex_size / 8;

struct PSShaderData {
	vec3 cam_up;
	float time;
	vec3 cam_fwd;
	float one_over_aspect_ratio;
	vec3 cam_right;
	float cam_zoom;
	vec3 cam_pos;
	float padding__0;
};

struct BrushPositionData {
	vec3 position;
	float padding__0;
	vec3 normal;
	float padding__1;
};

struct BrushFindData {
	vec3 pos;
	float depth;
	vec3 dir;
	float padding__0;
};

static void gfxErrorExit() {
	gfx::logD3D11messages();
	exit(1);
}

static Mesh makeFullScreenTriangle() {
	Mesh::Vertex verts[] = {
		{ vec3(-1, -1, 0), vec2(0, 0) },
		{ vec3(-1,  3, 0), vec2(0, 2) },
		{ vec3( 3, -1, 0), vec2(2, 0) },
	};

	Mesh::Index indices[] = {
		0, 1, 2,
	};

	Mesh m;
	if (!m.create(verts, indices)) {
		gfxErrorExit();
	}

	return m;
}

int main() {
	win::create("Honours Project", 800, 600);
	assert(all(maintex_size % 8 == 0));
	assert(all(brush_size % 8 == 0));

	{
		Camera cam;
		Brush brush;
		Texture3D main_texture;
		DynamicShader sh_manager;
		int main_ps_ind, main_vs_ind, sculpt_ind, gen_brush_ind, empty_texture_ind, find_brush_ind;
		Shader *main_ps, *main_vs, *sculpt, *gen_brush, *empty_texture, *find_brush;
		Handle<Buffer> brush_pos_data;
		//Buffer brush_pos_data;

		if (!main_texture.create(maintex_size, Texture3D::Type::float16)) gfxErrorExit();

		main_vs_ind       = sh_manager.add("base_vs",          ShaderType::Vertex);
		main_ps_ind       = sh_manager.add("base_ps",          ShaderType::Fragment);
		sculpt_ind        = sh_manager.add("sculpt_cs",        ShaderType::Compute);
		gen_brush_ind     = sh_manager.add("gen_brush_cs",     ShaderType::Compute);
		empty_texture_ind = sh_manager.add("empty_texture_cs", ShaderType::Compute);
		find_brush_ind    = sh_manager.add("find_brush_cs",    ShaderType::Compute);

		main_vs       = sh_manager.get(main_vs_ind);
		main_ps       = sh_manager.get(main_ps_ind);
		sculpt        = sh_manager.get(sculpt_ind);
		gen_brush     = sh_manager.get(gen_brush_ind);
		empty_texture = sh_manager.get(empty_texture_ind);
		find_brush    = sh_manager.get(find_brush_ind);

		if (!main_vs)       gfxErrorExit();
		if (!main_ps)       gfxErrorExit();
		if (!sculpt)        gfxErrorExit();
		if (!gen_brush)     gfxErrorExit();
		if (!empty_texture) gfxErrorExit();
		if (!find_brush)    gfxErrorExit();

		main_ps->addSampler();

		Handle<Buffer> shader_data_handle = Buffer::makeConstant<PSShaderData>(Buffer::Usage::Dynamic);
		Handle<Buffer> operation_data_handle = Buffer::makeConstant<OperationData>(Buffer::Usage::Dynamic);
		Handle<Buffer> find_data_handle = Buffer::makeConstant<BrushFindData>(Buffer::Usage::Dynamic);

		brush_pos_data = Buffer::makeStructured<BrushPositionData>();

		if (!shader_data_handle)    gfxErrorExit();
		if (!operation_data_handle) gfxErrorExit();
		if (!find_data_handle)      gfxErrorExit();
		if (!brush_pos_data)        gfxErrorExit();

		// int shader_data_ind = main_ps->addBuffer<PSShaderData>(Buffer::Usage::Dynamic);
		// int operation_data_ind = sculpt->addBuffer<OperationData>(Buffer::Usage::Dynamic);
		// int find_data_ind = find_brush->addBuffer<BrushFindData>(Buffer::Usage::Dynamic);
		// if (!brush_pos_data.createStructured<BrushPositionData>()) gfxErrorExit();

		Mesh triangle = makeFullScreenTriangle();

		// generate brush
		gen_brush->dispatch(brush_size / 8, {}, {}, { brush.getUAV()});

		// fill texture
		empty_texture->dispatch(threads, {}, {}, { main_texture.uav });

		GPUClock sculpt_clock("Sculpt");

		brush.setBuffer(operation_data_handle);
		Buffer *brush_pos_buf = brush_pos_data.get();
		assert(brush_pos_buf);

		if (Buffer *buf = operation_data_handle.get()) {
			if (OperationData *data = buf->map<OperationData>()) {
				data->operation = Operations::Union;
				data->smooth_k  = 0.f;
				data->scale     = 1.f;
				data->depth     = 0.f;
				buf->unmap();
			}
		}
		
		sculpt->dispatch(threads, { operation_data_handle }, { brush.getSRV(), brush_pos_buf->srv }, { main_texture.uav });

		while (win::isOpen()) {
			assert(brush_pos_buf == brush_pos_data.get());
			sh_manager.poll();

			if (sculpt_clock.isReady()) sculpt_clock.print();
			if (isActionPressed(Action::CloseProgram)) win::close();

			cam.update();
			brush.update();

			if (Buffer* buf = find_data_handle.get()) {
				if (BrushFindData *data = buf->map<BrushFindData>()) {
					data->pos   = cam.pos + cam.fwd * cam.getZoom();
					data->dir   = cam.getMouseDir();
					data->depth = brush.depth;
					buf->unmap();
				}
			}

			find_brush->dispatch(1, { find_data_handle, /*find_tex_data_ind*/ }, { main_texture.srv }, { brush_pos_buf->uav });

			if (cam.shouldSculpt()) {
				sculpt_clock.start();
				sculpt->dispatch(threads, { operation_data_handle }, { brush.getSRV(), brush_pos_buf->srv }, { main_texture.uav });
				sculpt_clock.end();
			}

			if (sh_manager.hasUpdated()) {
				is_dirty = true;
			}

			gfx::begin();

			if (Buffer *buf = shader_data_handle.get()) {
				if (PSShaderData *data = buf->map<PSShaderData>()) {
					const vec2 &resolution = Options::get().resolution;
					is_dirty = true;

					data->cam_pos = cam.pos;
					data->cam_fwd = cam.fwd;
					data->cam_right = cam.right;
					data->cam_up = cam.up;
					data->cam_zoom = cam.getZoom();
					data->one_over_aspect_ratio = resolution.y / resolution.x;
					data->time = win::timeSinceStart();
					buf->unmap();
				}
			}

			gfx::main_rtv.clear(Colour::dark_grey);
			gfx::main_rtv.bind();
			main_vs->bind();
			main_ps->bind();
			main_ps->bindCBuffers({ shader_data_handle, /*tex_data_ind*/ });
			main_ps->setSRV({ main_texture.srv, brush_pos_buf->srv });
			triangle.render();
			main_ps->setSRV({ nullptr, nullptr });
			main_ps->unbindCBuffers(2);

			gfx::imgui_rtv.bind();
				fpsWidget();
				mainTargetWidget();
				messagesWidget();
				drawLogger();
				brush.drawWidget();
			
			gfx::end();

			if (isActionPressed(Action::TakeScreenshot)) {
				gfx::main_rtv.takeScreenshot();
			}

			gfx::logD3D11messages();

			is_dirty = false;
		}
	}

	win::cleanup();
}
