#include "ray_tracing_editor.h"

#include <imgui.h>

#include "system.h"
#include "texture.h"
#include "buffer.h"
#include "shader.h"
#include "material_editor.h"
#include "widgets.h"

constexpr int block_size = 16;
constexpr int group_size = 8;
constexpr int skip_size = block_size * group_size;

RayTracingEditor::RayTracingEditor() {
	image       = Texture2D::create(gfx::main_rtv->size, true);
	data_handle = Buffer::makeConstant<RayTraceData>(Buffer::Usage::Dynamic);
	shader      = Shader::compile("ray_tracing_cs.hlsl", ShaderType::Compute);

	if (!image)                gfx::errorExit();
	if (!data_handle)          gfx::errorExit();
	if (!shader)               gfx::errorExit();
	if (!shader->addSampler()) gfx::errorExit();
}

bool RayTracingEditor::update(const MaterialEditor &me) {
	if (any(gfx::main_rtv->size != image->size)) {
		resize(gfx::main_rtv->size);
		reset();
	}

	// don't update if the window is not even visible
	if (!is_view_active) return false;

	if (data.thread_loc.y >= (uint)image->size.y) {
		// if we're not actively rendering, just do a rough first frame
		if (!is_rendering) return false;
		
		data.num_rendered_frames++;
		data.thread_loc = 0;
		sum_frame_times += rough_clock.getTime();
		rough_clock.begin();
	}

	if (RayTraceData *rt_data = data_handle->map<RayTraceData>()) {
		*rt_data = data;
		rt_data->num_of_lights = (uint)me.getLightsCount();
		data_handle->unmap();
	}

	data.thread_loc.x += skip_size;
	if (data.thread_loc.x >= (uint)image->size.x) {
		data.thread_loc.x = 0;
		data.thread_loc.y += skip_size;
	}

	return true;
}

void RayTracingEditor::widget() {
	imageWidget();
	timeWidget();
	editorWidget();
}

void RayTracingEditor::reset() {
	start_render = timerNow();
	data.thread_loc = 0;
	data.num_rendered_frames = 0;
	sum_frame_times = 0;
	rough_clock.begin();
	image->clear(Colour::black);
}

void RayTracingEditor::resize(const vec2i &size) {
	image->init(size, true);
}

void RayTracingEditor::step(MaterialEditor &me, Handle<Texture3D> main_tex, Handle<Buffer> shader_data) {
	shader->dispatch(
		vec3u(block_size, block_size, 1),
		{ data_handle, shader_data, me.getBuffer() },
			{
				main_tex->srv,
				me.getDiffuse(),
				me.getBackground(),
				me.getLights()->srv
			},
		{ image->uav }
	);
}

bool RayTracingEditor::isEditorOpen() const {
	return is_editor_open;
}

void RayTracingEditor::setEditorOpen(bool is_open) {
	is_editor_open = is_open;
}

bool RayTracingEditor::isViewOpen() const {
	return is_view_open;
}

void RayTracingEditor::setViewOpen(bool is_open) {
	is_view_open = is_open;
}

bool RayTracingEditor::isRendering() const {
	return is_rendering;
}

bool RayTracingEditor::shouldRedraw(bool is_dirty) const {
	return should_redraw || (refresh && is_dirty);
}

Handle<Texture2D> RayTracingEditor::getImage() const {
	return image;
}

Handle<Shader> RayTracingEditor::getShader() const {
	return shader;
}

void RayTracingEditor::imageWidget() {
	if (!is_view_open) return;
	if (!ImGui::Begin("Render", &is_view_open)) {
		is_view_active = false;
		ImGui::End();
		return;
	}

	is_view_active = true;

	widgets::imageView(image);

	ImGui::End();
}

void RayTracingEditor::timeWidget() {
	if (!is_rendering) return;

	constexpr float PAD = 10.0f;
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
	ImGuiViewport *viewport = ImGui::GetMainViewport();
	vec2 work_pos = viewport->WorkPos;
	vec2 work_size = viewport->WorkSize;
	vec2 window_pos = {
		work_pos.x + work_size.x - PAD,
		work_pos.y + PAD
	};
	vec2 window_pos_pivot = { 1.f, 0.f };
	ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::SetNextWindowBgAlpha(0.35f);

	ImGui::Begin("TIME overlay", nullptr, window_flags);
		ImGui::Text("Rendering time: %s", timerFormat(timerSince(start_render)));
		ImGui::Text("Frames rendered: %d", data.num_rendered_frames);
		ImGui::Text("Average time per frame: %s", data.num_rendered_frames ? timerFormat(sum_frame_times / data.num_rendered_frames) : "n/a");
	ImGui::End();
}

void RayTracingEditor::editorWidget() {
	if (!is_editor_open) return;
	if (!ImGui::Begin("Render Editor", &is_editor_open)) {
		ImGui::End();
		return;
	}

	const auto &btnFillWidth = [](const char *label, const char *tip = nullptr) {
		float width = ImGui::GetContentRegionAvail().x;
		if (tip) width -= 30.f;
		bool result = ImGui::Button(label, vec2(width, 0));
		if (tip) tooltip(tip);
		return result;
	};

	should_redraw = false;

	ImGui::PushItemWidth(-1);

	ImGui::Text("Number of steps");
	tooltip(
		"How many times the ray will be allowed to step forward,"
		" a lower number might not be able to pick up every object "
		"in exchange of faster rendering"
	);
	should_redraw |= inputUint("##max_steps", data.maximum_steps);

	ImGui::Text("Number of bounces");
	tooltip("How many times light is allowed to bounce");
	should_redraw |= inputUint("##max_bounces", data.maximum_bounces);

	ImGui::Text("Number of rays per pixel");
	tooltip(
		"Every frame, this number of rays are shot at each pixel "
		"for the calculation, an higher number will need less "
		"frames to render the complete image, but each frame will "
		"take a bit longer"
	);
	should_redraw |= inputUint("##max_rays", data.maximum_rays);

	ImGui::Text("Trace distance");
	tooltip("How far away is a ray allowed to go");
	should_redraw |= ImGui::DragFloat("##trace_distance", &data.maximum_trace_dist, 10.f);

	ImGui::Text("Keep refreshing");
	tooltip("Refresh the render any time something changes");
	ImGui::SameLine();
	ImGui::Checkbox("##refresh", &refresh);

	ImGui::BeginDisabled(refresh);
	should_redraw |= btnFillWidth("Render quick", "Do a quick render of the scene");
	ImGui::EndDisabled();

	if (btnFillWidth(is_rendering ? "Stop render" : "Resume render", is_rendering ? "Stop rendering the scene" : "Resume rendering the scene")) {
		is_rendering = !is_rendering;
		rough_clock.begin();
	}

	if (btnFillWidth("Save image", "Save the current frame to a file called \"ray_tracing_render_XYZ.png\" in the screenshots folder")) {
		is_rendering = false;
		image->takeScreenshot("ray_tracing_render");
	}

	ImGui::PopItemWidth();

	ImGui::End();
}
