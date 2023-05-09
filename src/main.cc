#include <stdio.h>

#include "system.h"
#include "input.h"
#include "options.h"
#include "tracelog.h"
#include "widgets.h"
#include "timer.h"
#include "camera.h"
#include "brush_editor.h"
#include "material_editor.h"

#include "buffer.h"
#include "colour.h"
#include "mesh.h"
#include "shader.h"
#include "texture.h"
#include "ray_tracing_editor.h"
#include "sculpture.h"

#include <imgui.h>
#include <d3d11.h>

struct PSShaderData {
	vec3 cam_up;
	float time;
	vec3 cam_fwd;
	float one_over_aspect_ratio;
	vec3 cam_right;
	float cam_zoom;
	vec3 cam_pos;
	uint num_of_lights;
	uint use_tonemapping;
	float exposure_bias;
	vec2 padding__0;
};

GFX_CLASS_CHECK(PSShaderData);

static Mesh makeFullScreenTriangle();
static void setImGuiTheme();

int main() {
	const char *base_name = "Honours Project";
	win::create(base_name, 800, 600);

	// push stack so it cleans up after itself before closing
	{
		CPUClock init_timer("initialization");

		Camera cam;
		BrushEditor brush_editor;
		MaterialEditor material_editor;
		RayTracingEditor rt_editor;
		Sculpture sculpture = Sculpture(brush_editor);
		Options &options = Options::get();
		bool is_dirty = false;

		Handle<Shader> main_vs            = Shader::compile("main_vs.hlsl",          ShaderType::Vertex);
		Handle<Shader> main_ps            = Shader::compile("main_ps.hlsl",          ShaderType::Fragment);
		Handle<Buffer> shader_data_handle = Buffer::makeConstant<PSShaderData>(Buffer::Usage::Dynamic);
		Mesh triangle                     = makeFullScreenTriangle();

		if (!main_vs)            gfx::errorExit();
		if (!main_ps)            gfx::errorExit();
		if (!shader_data_handle) gfx::errorExit();

		main_ps->addSampler();

		widgets::setupMenuBar(brush_editor, material_editor, rt_editor, sculpture);
		setImGuiTheme();

		init_timer.print();

		while (win::isOpen()) {
			widgets::saveLoadFile();
			sculpture.update();

			if (isActionPressed(Action::CaptureFrame)) {
				gfx::captureFrame();
			}

			if (isActionPressed(Action::CloseProgram)) {
				win::close();
			}

			if (rt_editor.update(material_editor)) {
				rt_editor.step(material_editor, sculpture.texture, shader_data_handle);
			}

			is_dirty |= Shader::hasUpdated(rt_editor.getShader());
			is_dirty |= cam.update();
			brush_editor.update();
			is_dirty |= material_editor.update();

			if (rt_editor.shouldRedraw(is_dirty)) {
				is_dirty = false;
				// win::setWindowName(str::format("%s-%s*", base_name, project_name.get()));
				rt_editor.reset();
			}

			if (gfx::isMainRTVActive()) {
				brush_editor.findBrush(cam, sculpture.texture);


				if (cam.shouldSculpt()) {
					is_dirty = true;
					sculpture.runSculpt();
					win::setWindowName(str::format("%s - %s*", base_name, sculpture.getName()));
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
					data->use_tonemapping = (uint)material_editor.useTonemapping();
					data->exposure_bias = material_editor.getExposure();
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
					sculpture.texture->srv,
					material_editor.getDiffuse(),
					material_editor.getBackground(),
					material_editor.getLights()->srv
				}
			);
			triangle.render();
			main_ps->unbind(2, 5);
			main_ps->unbindCBuffers(2);

			gfx::imgui_rtv->bind();
				if (options.show_fps) widgets::fps();
				widgets::menuBar();
				rt_editor.widget();
				widgets::mainView();
				widgets::messages();
				widgets::keyRemapper();
				widgets::controlsPage();
				drawLogger();
				brush_editor.drawWidget(sculpture.texture);
				material_editor.drawWidget();
				options.drawWidget();
			
			gfx::end();

			if (isActionPressed(Action::TakeScreenshot)) {
				if (gfx::isMainRTVActive())       gfx::main_rtv->takeScreenshot();
				else if (rt_editor.isRendering()) rt_editor.getImage()->takeScreenshot();
				else widgets::addMessage(LogLevel::Error, "Trying to take a screenshot but neither the main nor the ray tracing widget are selected");
			}

			gfx::logD3D11messages();
		}
	}

	win::cleanup();
}

static Mesh makeFullScreenTriangle() {
	Mesh::Vertex verts[] = {
		{ vec3(-1, -1, 0), vec2(0, 0) },
		{ vec3(-1,  3, 0), vec2(0, 2) },
		{ vec3(3, -1, 0), vec2(2, 0) },
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

static void setImGuiTheme() {
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