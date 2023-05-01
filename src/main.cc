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
#include "ray_tracing_editor.h"

#include <imgui.h>

#include <d3d11.h>

#include "utils.h"

//#include <tracy/TracyD3D11.hpp>

constexpr vec3u maintex_size = 512;

static_assert(all(maintex_size % 8 == 0));

struct PSShaderData {
	vec3 cam_up;
	float time;
	vec3 cam_fwd;
	float one_over_aspect_ratio;
	vec3 cam_right;
	float cam_zoom;
	vec3 cam_pos;
	uint num_of_lights;
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

static void setTheme();

int main() {
	win::create("Honours Project", 800, 600);

	// push stack so it cleans up after itself before closing
	{
		Camera cam;
		BrushEditor brush_editor;
		MaterialEditor material_editor;
		DynamicShader sh_manager;
		RayTracingEditor rt_editor = sh_manager;

		setTheme();

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

		static bool rt_is_dirty = false;

		while (win::isOpen()) {
			sh_manager.poll();
			saveLoadFileDialog(main_texture, scale_cs);

			if (isActionPressed(Action::CaptureFrame)) {
				gfx::captureFrame();
			}

			if (isActionPressed(Action::CloseProgram)) {
				win::close();
			}

			if (rt_editor.update(material_editor)) {
				rt_editor.step(material_editor, main_texture, shader_data_handle);
			}

			rt_is_dirty |= sh_manager.hasUpdated();
			rt_is_dirty |= cam.update();
			brush_editor.update();
			rt_is_dirty |= material_editor.update();

			if (rt_editor.shouldRedraw(rt_is_dirty)) {
				rt_is_dirty = false;
				rt_editor.reset();
			}

			if (gfx::isMainRTVActive()) {
				if (Buffer *buf = find_data_handle.get()) {
					if (BrushFindData *data = buf->map<BrushFindData>()) {
						data->pos = cam.pos + cam.fwd * cam.getZoom();
						data->dir = cam.getMouseDir();
						data->depth = brush_editor.getDepth();
						data->scale = brush_editor.getScale();
						buf->unmap();
					}
				}

				find_brush->dispatch(1, { find_data_handle }, { main_texture->srv }, { brush_editor.getDataUAV() });

				if (cam.shouldSculpt()) {
					rt_is_dirty = true;
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
					data->num_of_lights = (uint)material_editor.getLightsCount();
					buf->unmap();
				}
			}

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
					material_editor.getLights()->srv
				}
			);
			triangle.render();
			main_ps->unbind(2, 5);
			main_ps->unbindCBuffers(2);

			gfx::imgui_rtv->bind();
				if (options.show_fps) fpsWidget();
				mainMenuBar(brush_editor, material_editor, main_texture);
				rt_editor.widget();
				mainViewWidget();
				messagesWidget();
				keyRemapper();
				drawLogger();
				brush_editor.drawWidget();
				material_editor.drawWidget();
				options.drawWidget();
			
			gfx::end();

			if (isActionPressed(Action::TakeScreenshot)) {
				if (gfx::isMainRTVActive())       gfx::main_rtv->takeScreenshot();
				else if (rt_editor.isRendering()) rt_editor.getImage()->takeScreenshot();
				else addMessageToWidget(LogLevel::Error, "Trying to take a screenshot but neither the main nor the ray tracing widget are selected");
			}

			gfx::logD3D11messages();
		}
	}

	win::cleanup();
}

static void setTheme() {
	// from https://github.com/OverShifted/OverEngine
	ImGuiStyle& style = ImGui::GetStyle();
	style.Colors[ImGuiCol_Text]                  = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	style.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
	style.Colors[ImGuiCol_ChildBg]               = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
	style.Colors[ImGuiCol_PopupBg]               = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
	style.Colors[ImGuiCol_Border]                = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
	style.Colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	style.Colors[ImGuiCol_FrameBg]               = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
	style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
	style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.67f, 0.67f, 0.67f, 0.39f);
	style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
	style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
	style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
	style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	style.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
	style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
	style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
	style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
	style.Colors[ImGuiCol_CheckMark]             = ImVec4(0.11f, 0.64f, 0.92f, 1.00f);
	style.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.11f, 0.64f, 0.92f, 1.00f);
	style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.08f, 0.50f, 0.72f, 1.00f);
	style.Colors[ImGuiCol_Button]                = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
	style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
	style.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.67f, 0.67f, 0.67f, 0.39f);
	style.Colors[ImGuiCol_Header]                = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
	style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
	style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.67f, 0.67f, 0.67f, 0.39f);
	style.Colors[ImGuiCol_Separator]             = style.Colors[ImGuiCol_Border];
	style.Colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.41f, 0.42f, 0.44f, 1.00f);
	style.Colors[ImGuiCol_SeparatorActive]       = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
	style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	style.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.29f, 0.30f, 0.31f, 0.67f);
	style.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
	style.Colors[ImGuiCol_Tab]                   = ImVec4(0.08f, 0.08f, 0.09f, 0.83f);
	style.Colors[ImGuiCol_TabHovered]            = ImVec4(0.33f, 0.34f, 0.36f, 0.83f);
	style.Colors[ImGuiCol_TabActive]             = ImVec4(0.23f, 0.23f, 0.24f, 1.00f);
	style.Colors[ImGuiCol_TabUnfocused]          = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
	style.Colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
	style.Colors[ImGuiCol_DockingPreview]        = ImVec4(0.26f, 0.59f, 0.98f, 0.70f);
	style.Colors[ImGuiCol_DockingEmptyBg]        = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
	style.Colors[ImGuiCol_PlotLines]             = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
	style.Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	style.Colors[ImGuiCol_PlotHistogram]         = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	style.Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
	style.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
	style.Colors[ImGuiCol_DragDropTarget]        = ImVec4(0.11f, 0.64f, 0.92f, 1.00f);
	style.Colors[ImGuiCol_NavHighlight]          = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	style.Colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
	style.Colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
	style.GrabRounding                           = style.FrameRounding = 2.3f;
}