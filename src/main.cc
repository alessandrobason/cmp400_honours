#include <stdio.h>

#include "system.h"
#include "input.h"
#include "maths.h"
#include "options.h"
#include "tracelog.h"
#include "widgets.h"
#include "timer.h"
#include "shape_builder.h"
#include "camera.h"
#include "matrix.h"
#include "brush_editor.h"
#include "material_editor.h"

#include "buffer.h"
#include "colour.h"
#include "mesh.h"
#include "shader.h"
#include "texture.h"

#include <imgui.h>

#include <d3d11.h>

#include "utils.h"

//#include <tracy/TracyD3D11.hpp>

constexpr vec3u maintex_size = 512;
constexpr unsigned int initial_material_count = 10;

static_assert(all(maintex_size % 8 == 0));

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

struct BrushFindData {
	vec3 pos;
	float depth;
	vec3 dir;
	float scale;
};

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
		gfx::errorExit();
	}

	return m;
}

constexpr int block_size = 16;
constexpr int group_size = 8;
constexpr int skip_size = block_size * group_size;

struct RayTracing {
	struct RayTraceData {
		vec2u thread_loc;
		unsigned int num_rendered_frames;
		unsigned int padding;
	};

	RayTracing(DynamicShader &ds) {
		image = Texture2D::create(gfx::main_rtv->size, true);
		rt_data = Buffer::makeConstant<RayTraceData>(Buffer::Usage::Dynamic);
		shader  = ds.add("ray_tracing_cs", ShaderType::Compute);

		if (!image)                gfx::errorExit();
		if (!rt_data)              gfx::errorExit();
		if (!shader)               gfx::errorExit();
		if (!shader->addSampler()) gfx::errorExit();
	}

	bool update() {
		if (any(gfx::main_rtv->size != image->size)) {
			resize(gfx::main_rtv->size);
			reset();
		}

		if (thread_loc.y >= (unsigned int)image->size.y) {
			rendered_frames++;
			thread_loc = 0;
		}

		if (RayTraceData *data = rt_data->map<RayTraceData>()) {
			data->thread_loc = thread_loc;
			data->num_rendered_frames = rendered_frames;
			rt_data->unmap();
		}

		thread_loc.x += skip_size;
		if (thread_loc.x >= (unsigned int)image->size.x) {
			thread_loc.x = 0;
			thread_loc.y += skip_size;
		}

		return true;
	}

	void reset() {
		thread_loc = 0;
		rendered_frames = 0;
		image->clear(Colour::black);
	}

	void resize(const vec2i &size) {
		image->init(size, true);
	}

	void step(
		MaterialEditor &me,
		Handle<Texture3D> main_tex,
		Handle<Buffer> shader_data
	) {

		shader->dispatch(
			vec3u(block_size, block_size, 1),
			{ rt_data, shader_data, me.getBuffer(), me.light_buf },
			{
				main_tex->srv,
				me.getDiffuse(),
				me.getBackground(),
				me.getIrradiance()
			},
			{ image->uav }
		);
	}

	Handle<Shader> shader;
	Handle<Texture2D> image;
	Handle<Buffer> rt_data;
	vec2u thread_loc = 0;
	unsigned int rendered_frames = 0;
};

int main() {
	win::create("Honours Project", 800, 600);

	// push stack so it cleans up after itself before closing
	{
		Camera cam;
		BrushEditor brush_editor;
		MaterialEditor material_editor;
		DynamicShader sh_manager;
		RayTracing ray_tracing = sh_manager;

		Handle<Texture3D> main_texture = Texture3D::create(maintex_size, Texture3D::Type::float16);
		brush_editor.runFillShader(Shapes::Sphere, ShapeData(vec3(0), 21), main_texture);

		if (!main_texture) gfx::errorExit();

		Handle<Shader> main_vs       = sh_manager.add("base_vs",          ShaderType::Vertex);
		Handle<Shader> main_ps       = sh_manager.add("base_ps",          ShaderType::Fragment);
		Handle<Shader> sculpt        = sh_manager.add("sculpt_cs",        ShaderType::Compute);
		Handle<Shader> find_brush    = sh_manager.add("find_brush_cs",    ShaderType::Compute);
		Handle<Shader> scale_cs      = sh_manager.add("scale_cs", ShaderType::Compute);

		if (!main_vs)       gfx::errorExit();
		if (!main_ps)       gfx::errorExit();
		if (!sculpt)        gfx::errorExit();
		if (!find_brush)    gfx::errorExit();
		if (!scale_cs)      gfx::errorExit();

		main_ps->addSampler();

		Handle<Buffer> shader_data_handle    = Buffer::makeConstant<PSShaderData>(Buffer::Usage::Dynamic);
		Handle<Buffer> find_data_handle      = Buffer::makeConstant<BrushFindData>(Buffer::Usage::Dynamic);

		if (!shader_data_handle)    gfx::errorExit();
		if (!find_data_handle)      gfx::errorExit();

		Mesh triangle = makeFullScreenTriangle();
		Options &options = Options::get();

		static bool is_rt = false;
		static bool rt_is_dirty = false;

		while (win::isOpen()) {
			sh_manager.poll();
			saveLoadFileDialog(main_texture, scale_cs);

			if (isKeyPressed(KEY_F)) {
				gfx::captureFrame();
			}

			if (isKeyPressed(KEY_R)) {
				is_rt = !is_rt;
				rt_is_dirty |= is_rt;
				if (is_rt) ray_tracing.reset();
			}

			if (isActionPressed(Action::CloseProgram)) {
				win::close();
			}

			

			if (is_rt && ray_tracing.update()) {
				ray_tracing.step(material_editor, main_texture, shader_data_handle);
			}

			rt_is_dirty |= sh_manager.hasUpdated();
			rt_is_dirty |= cam.update();
			brush_editor.update();
			rt_is_dirty |= material_editor.update();

			if (is_rt && rt_is_dirty) {
				rt_is_dirty = false;
				ray_tracing.reset();
			}

			if (!is_rt) {
				if (Buffer* buf = find_data_handle.get()) {
					if (BrushFindData *data = buf->map<BrushFindData>()) {
						data->pos   = cam.pos + cam.fwd * cam.getZoom();
						data->dir   = cam.getMouseDir();
						data->depth = brush_editor.getDepth();
						data->scale = brush_editor.getScale();
						buf->unmap();
					}
				}

				find_brush->dispatch(1, { find_data_handle }, { main_texture->srv }, { brush_editor.getDataUAV() });

				if (cam.shouldSculpt()) {
					if (options.auto_capture) {
						gfx::captureFrame();
					}
					sculpt->dispatch(main_texture->size / 8, { brush_editor.getOperHandle() }, { brush_editor.getBrushSRV(), brush_editor.getDataSRV() }, { main_texture->uav });
				}
			}

			gfx::begin();

			if (Buffer *buf = shader_data_handle.get()) {
				if (PSShaderData *data = buf->map<PSShaderData>()) {
					const vec2 &resolution = options.resolution;

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

			if (!is_rt) {
				gfx::main_rtv->clear(Colour::dark_grey);
				gfx::main_rtv->bind();
				main_vs->bind();
				main_ps->bind(
					{ shader_data_handle, material_editor.getBuffer() },
					{
						brush_editor.getDataSRV(),
						main_texture->srv,
						material_editor.getDiffuse(),
						material_editor.getBackground(),
						material_editor.getIrradiance()
					}
				);
				triangle.render();
				main_ps->unbind(2, 5);
				main_ps->unbindCBuffers(2);
			}

			gfx::imgui_rtv->bind();
				if (options.show_fps) fpsWidget();
				mainMenuBar(brush_editor, material_editor, main_texture);
				if (is_rt) mainTargetWidget(ray_tracing.image);
				else       mainTargetWidget(gfx::main_rtv);
				messagesWidget();
				keyRemapper();
				drawLogger();
				brush_editor.drawWidget();
				material_editor.drawWidget();
				options.drawWidget();
			
			gfx::end();

			if (isActionPressed(Action::TakeScreenshot)) {
				is_rt ? ray_tracing.image->takeScreenshot() : gfx::main_rtv->takeScreenshot();
			}

			gfx::logD3D11messages();
		}
	}

	win::cleanup();
}
