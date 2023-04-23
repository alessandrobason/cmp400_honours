#include "brush.h"

#include <d3d11.h>
#include <imgui.h>
#include "system.h"
#include "input.h"
#include "widgets.h"
#include "buffer.h"

constexpr vec3u brush_tex_size = 64;
constexpr Texture3D::Type brush_type = Texture3D::Type::float16;

Operations operator|=(Operations &a, Operations b) {
	a = (Operations)((uint32_t)a | (uint32_t)b);
	return a;
}

static const Operations state_to_oper[(int)Brush::State::Count] = {
	Operations::Union,	     // Brush 
	Operations::Subtraction, // Eraser
};

static void centreButton(float width, float padding);

static void gfxErrorExit(const char *ctx) {
	err("Failed to %s", ctx);
	gfx::logD3D11messages();
	exit(1);
}

Brush::Brush() {
	if (!texture.create(brush_tex_size, brush_type)) {
		gfxErrorExit("initialize brush");
	}

	if (!brush_icon.load("assets/brush_icon.png")) {
		fatal("couldn't load brush icon");
	}

	if (!eraser_icon.load("assets/eraser_icon.png")) {
		fatal("couldn't load eraser icon");
	}
}

void Brush::setBuffer(Handle<Buffer> handle) {
	assert(handle.isValid());
	buf_handle = handle;
}

void Brush::drawWidget() {
	if (!ImGui::Begin("Brush", &is_open)) {
		ImGui::End();
		return;
	}

	// use a table so the two buttons are centred

	ImGuiTableFlags table_flags = 0;
	constexpr vec2 brush_size = 24;
	ImGuiStyle &style = ImGui::GetStyle();
	const vec2 &btn_padding = ImGui::GetStyle().FramePadding;
	const vec4 &btn_active_col = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);;
	const vec4 &btn_base_col = ImGui::GetStyleColorVec4(ImGuiCol_Button);;

	ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, vec2(0));

	if (ImGui::BeginTable("brush and eraser", 2, table_flags)) {
		ImGui::TableNextRow();

		ImGui::TableSetColumnIndex(0);

		centreButton(brush_size.x, btn_padding.x);

		style.Colors[ImGuiCol_Button] = state == State::Brush ? btn_active_col : btn_base_col;

		if (ImGui::ImageButton((ImTextureID)brush_icon.srv, brush_size)) {
			has_changed |= state != State::Brush;
			state = State::Brush;
		}

		ImGui::TableSetColumnIndex(1);

		centreButton(brush_size.x, btn_padding.x);

		style.Colors[ImGuiCol_Button] = state == State::Eraser ? btn_active_col : btn_base_col;

		if (ImGui::ImageButton((ImTextureID)eraser_icon.srv, brush_size)) {
			has_changed |= state != State::Eraser;
			state = State::Eraser;
		}

		style.Colors[ImGuiCol_Button] = btn_base_col;

		ImGui::EndTable();
	}

	ImGui::Text("Scale");
	has_changed |= filledSlider("##Size", &scale, 1.f, 5.f, "%.1f");
	ImGui::Text("Depth");
	has_changed |= filledSlider("##Depth", &depth, -1.f, 1.f);
	ImGui::Text("Smooth constant");
	has_changed |= filledSlider("##Smooth", &smooth_k, 0.f, 20.f);
	ImGui::Text("Single click");
	ImGui::Checkbox("##Single click", &is_single_click);
	ImGui::Text("Material");
	has_changed |= inputU8("##MatIndex", &material_index);

	ImGui::PopStyleVar();

	ImGui::End();

	if (isKeyDown(KEY_Q)) depth -= 2.f * win::dt;
	if (isKeyDown(KEY_W)) depth += 2.f * win::dt;
}

void Brush::update() {
	if (!has_changed) return;
	if (Buffer *buf = buf_handle.get()) {
		if (OperationData *data = buf->map<OperationData>()) {
			data->operation = (uint32_t)state_to_oper[(int)state];
			if (smooth_k > 0.f) {
				data->operation |= (uint32_t)Operations::Smooth;
			}
			// data->operation |= material_index;
			data->smooth_k = smooth_k;
			data->scale = scale;
			data->colour = material_index > 1 ? vec3(0.1f, 1.0f, 0.2f) : vec3(1.0f, 0.1f, 0.2f);
			buf->unmap();
		}
	}
	has_changed = false;
}

ID3D11UnorderedAccessView *Brush::getUAV() {
	return texture.uav;
}

ID3D11ShaderResourceView *Brush::getSRV() {
	return texture.srv;
}

// == PRIVATE FUNCTIONS ========================================

static void centreButton(float width, float padding) {
	const float cursor_pos = ImGui::GetCursorPosX();
	const float column_width = ImGui::GetContentRegionAvail().x;
	const float brush_width = width + padding * 2.f;
	const float offset_x = (column_width - brush_width) * 0.5f;

	ImGui::SetCursorPosX(cursor_pos + offset_x);
}
