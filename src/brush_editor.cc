#include "brush_editor.h"

#include <d3d11.h>
#include <imgui.h>
#include <nfd.hpp>
#include "system.h"
#include "input.h"
#include "widgets.h"
#include "buffer.h"
#include "shader.h"
#include "camera.h"

constexpr vec3u brush_tex_size = 64;
constexpr Texture3D::Type brush_type = Texture3D::Type::float16;
constexpr const char *shape_macros[(int)Shapes::Count] = { "SHAPE_SPHERE", "SHAPE_BOX", "SHAPE_CYLINDER", nullptr };

static_assert(all(brush_tex_size % 8 == 0));

ShapeData::ShapeData(const vec3 &p, float x, float y, float z, float w) {
	position = p;
	data = vec4(x, y, z, w);
}

Operations operator|=(Operations &a, Operations b) {
	a = (Operations)((uint32_t)a | (uint32_t)b);
	return a;
}

static const Operations state_to_oper[(int)BrushEditor::State::Count] = {
	Operations::Union,	     // Brush
	Operations::Subtraction, // Eraser
};

static void centreButton(float width, float padding);

BrushEditor::BrushEditor() {
	brush_icon        = Texture2D::load("assets/GUI/brush_icon.png");
	eraser_icon       = Texture2D::load("assets/GUI/eraser_icon.png");
	depth_tooltips[0] = Texture2D::load("assets/GUI/depth_inside_tip.png");
	depth_tooltips[1] = Texture2D::load("assets/GUI/depth_normal_tip.png");
	depth_tooltips[2] = Texture2D::load("assets/GUI/depth_outside_tip.png");
	oper_handle       = Buffer::makeConstant<OperationData>(Buffer::Usage::Dynamic);
	fill_buffer       = Buffer::makeConstant<ShapeData>(Buffer::Usage::Dynamic);
	find_data_handle  = Buffer::makeConstant<BrushFindData>(Buffer::Usage::Dynamic);
	data_handle       = Buffer::makeStructured<BrushData>();
	find_brush        = Shader::compile("find_brush_cs.hlsl", ShaderType::Compute);

	if (!brush_icon)       gfx::errorExit("failed to load brush icon");
	if (!eraser_icon)      gfx::errorExit("failed to load eraser icon");
	if (!oper_handle)      gfx::errorExit("failed to create operation buffer");
	if (!find_data_handle) gfx::errorExit("failed to create find data buffer");
	if (!data_handle)      gfx::errorExit("failed to create data buffer");
	if (!find_brush)       gfx::errorExit("failed to compile find brush shader");

	for (int i = 0; i < (int)Shapes::Count; ++i) {
		fill_shaders[i] = Shader::compile(
			"fill_texture_cs.hlsl",
			ShaderType::Compute,
			{ { shape_macros[i]}, {nullptr} },
			false
		);
		if (!fill_shaders[i]) gfx::errorExit("couldn't compile fill shader");
	}

	addBrush("Sphere", Shapes::Sphere, ShapeData(vec3(0), 21));
	addBrush("Box", Shapes::Box, ShapeData(vec3(0), 42, 42, 42));
	addBrush("Cylinder", Shapes::Cylinder, ShapeData(vec3(0), 21, 42));
}

void BrushEditor::drawWidget(Handle<Texture3D> main_tex) {
	mouseWidget(main_tex);

	if (isActionPressed(Action::ChangeToBrush))  setState(State::Brush);
	if (isActionPressed(Action::ChangeToEraser)) setState(State::Eraser);

	if (!is_open) return;
	if (!ImGui::Begin("Brush Editor", &is_open)) {
		ImGui::End();
		return;
	}

	// use a table so the two buttons are centred

	ImGuiTableFlags table_flags = 0;
	constexpr vec2 brush_size = 24;
	ImGuiStyle &style = ImGui::GetStyle();
	const vec2 &btn_padding = ImGui::GetStyle().FramePadding;
	const vec4 &btn_active_col = ImGui::GetStyleColorVec4(ImGuiCol_SliderGrab);
	const vec4 &btn_base_col = ImGui::GetStyleColorVec4(ImGuiCol_Button);

	ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, vec2(0));

	if (ImGui::BeginTable("brush and eraser", 2, table_flags)) {
		ImGui::TableNextRow();

		ImGui::TableSetColumnIndex(0);

		centreButton(brush_size.x, btn_padding.x);

		style.Colors[ImGuiCol_Button] = state == State::Brush ? btn_active_col : btn_base_col;

		if (ImGui::ImageButton((ImTextureID)brush_icon->srv, brush_size)) {
			setState(State::Brush);
		}

		ImGui::TableSetColumnIndex(1);

		centreButton(brush_size.x, btn_padding.x);

		style.Colors[ImGuiCol_Button] = state == State::Eraser ? btn_active_col : btn_base_col;

		if (ImGui::ImageButton((ImTextureID)eraser_icon->srv, brush_size)) {
			setState(State::Eraser);
		}

		style.Colors[ImGuiCol_Button] = btn_base_col;

		ImGui::EndTable();
	}

	const auto &depthTip = [](Handle<Texture2D> handles[3]) {
		ImGui::SameLine();
		ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
			ImGui::BeginTooltip();
				ImGui::Text("The brush's depth relative to the surface");
				ImGui::Image((ImTextureID)handles[0]->srv, vec2(100));
				ImGui::SameLine();
				ImGui::Image((ImTextureID)handles[1]->srv, vec2(100));
				ImGui::SameLine();
				ImGui::Image((ImTextureID)handles[2]->srv, vec2(100));
			ImGui::EndTooltip();
		}
	};

	ImGui::Text("Scale");
	has_changed |= filledSlider("##Size", &scale, 1.f, 5.f, "%.1f");
	ImGui::Text("Depth");
	depthTip(depth_tooltips);
	has_changed |= filledSlider("##Depth", &depth, -1.5f, 1.5f);
	ImGui::Text("Blend amount");
	has_changed |= filledSlider("##Smooth", &smooth_k, 0.f, 20.f);

	ImGui::Text("Current brush");
	if (ImGui::BeginCombo("##Brushes", textures[brush_index].name.get())) {
		for (size_t i = 0; i < textures.len; ++i) {
			bool is_selected = brush_index == i;
			if (ImGui::Selectable(textures[i].name.get(), is_selected)) {
				brush_index = i;
			}

			if (is_selected) {
				ImGui::SetItemDefaultFocus();
			}
		}

		ImGui::EndCombo();
	}

	if (ImGui::Button("Load brush from file")) {
		should_open_nfd = true;
	}

	ImGui::PopStyleVar();

	ImGui::End();
}

void BrushEditor::update() {
	if (should_open_nfd) {
		should_open_nfd = false;

		NFD::UniquePathU8 path;
		nfdu8filteritem_t filter[] = { { "Tex3D", "bin" } };
		nfdresult_t result = NFD::OpenDialog(path, filter, ARRLEN(filter));
		if (result == NFD_OKAY) {
			brush_index = addTexture(path.get());
		}
		else if (result == NFD_CANCEL) {
			// user has closed the dialog without selecting
		}
		else {
			err("error with the file dialog: %s", NFD::GetError());
		}
	}

	if (!has_changed) return;
	if (Buffer *buf = oper_handle.get()) {
		if (OperationData *data = buf->map<OperationData>()) {
			data->operation = (uint32_t)state_to_oper[(int)state];
			if (smooth_k > 0.f) {
				data->operation |= (uint32_t)Operations::Smooth;
			}
			data->smooth_k = smooth_k;
			data->scale = scale;
			buf->unmap();
		}
	}
	has_changed = false;
}

void BrushEditor::findBrush(const Camera &cam, Handle<Texture3D> texture) {
	cam_pos = cam.pos + cam.fwd * cam.getZoom();
	cam_dir = cam.getMouseDir();

	if (BrushFindData *data = find_data_handle->map<BrushFindData>()) {
		data->pos = cam.pos + cam.fwd * cam.getZoom();
		data->dir = cam.getMouseDir();
		data->depth = depth;
		data->scale = getScale();
		find_data_handle->unmap();
	}

	find_brush->dispatch(1, { find_data_handle }, { texture->srv }, { data_handle->uav });
}

void BrushEditor::setOpen(bool new_is_open) {
	is_open = new_is_open;
}

bool BrushEditor::isOpen() const {
	return is_open;
}

ID3D11UnorderedAccessView *BrushEditor::getBrushUAV() {
	if (Texture3D *brush = get(brush_index)) return brush->uav;
	return nullptr;
}

ID3D11ShaderResourceView *BrushEditor::getBrushSRV() {
	if (Texture3D *brush = get(brush_index)) return brush->srv;
	return nullptr;
}

ID3D11UnorderedAccessView *BrushEditor::getDataUAV() {
	return data_handle->uav;
}

ID3D11ShaderResourceView *BrushEditor::getDataSRV() {
	return data_handle->srv;
}

vec3i BrushEditor::getBrushSize() const {
	if (const Texture3D *brush = get(brush_index)) return brush->size;
	return 0;
}

float BrushEditor::getScale() const {
	constexpr float base_brush_size = 64.f;
	if (const Texture3D *brush = get(brush_index)) {
		const float size_scale = (float)brush->size.x / base_brush_size;
		return scale * size_scale;
	}
	else {
		return scale;
	}
}

Handle<Buffer> BrushEditor::getOperHandle() {
	return oper_handle;
}

void BrushEditor::runFillShader(Shapes shape, const ShapeData &shape_data, Handle<Texture3D> destination) {
	if (shape != Shapes::None) {
		if (ShapeData *data = fill_buffer->map<ShapeData>()) {
			*data = shape_data;
			fill_buffer->unmap();
		}
	}
	
	fill_shaders[(int)shape]->dispatch(destination->size / 8, { fill_buffer }, {}, { destination->uav });
}

Texture3D *BrushEditor::get(size_t index) {
	return index < textures.len ? textures[index].handle.get() : nullptr;
}

const Texture3D *BrushEditor::get(size_t index) const {
	return index < textures.len ? textures[index].handle.get() : nullptr;
}

size_t BrushEditor::addTexture(const char *path) {
	str::view name = file::getFilename(path);

	size_t index = checkTextureAlreadyLoaded(name);
	if (index != -1) {
		warn("texture %s is already loaded", name.data);
		return index;
	}

	Handle<Texture3D> newtex = Texture3D::load(path);
	if (!newtex) {
		err("couldn't load texture %s", name.data);
		return -1;
	}

	index = textures.len;
	textures.push(newtex, name.dup());
	return index;
}

size_t BrushEditor::addBrush(const char *name, Shapes shape, const ShapeData &data) {
	Handle<Texture3D> newtex = Texture3D::create(brush_tex_size, brush_type);
	runFillShader(shape, data, newtex);
	size_t index = textures.len;
	textures.push(newtex, str::dup(name));
	return index;
}

size_t BrushEditor::checkTextureAlreadyLoaded(str::view name) {
	for (size_t i = 0; i < textures.len; ++i) {
		if (name == textures[i].name.get()) {
			return i;
		}
	}
	return -1;
}

void BrushEditor::mouseWidget(Handle<Texture3D> main_tex) {
	static bool is_menu_open = false;

	if (gfx::isMainRTVActive() || is_menu_open) {
		bool shift = isKeyDown(KEY_SHIFT);
		bool ctrl = isKeyDown(KEY_CTRL);
		bool alt = isKeyDown(KEY_ALT);

		const auto &beginMenu = [](const char *name, bool &is_open, float &value, float minv, float maxv, const char *tip = nullptr) {
			if (!is_open) {
				is_open = true;
				ImGuiStyle &style = ImGui::GetStyle();
				vec2 pos = getMousePos() + vec2(6, -8) * style.MouseCursorScale;
				ImGui::SetNextWindowPos(pos);
			}

			ImGui::SetNextWindowSize(vec2(200, 55));
			ImGui::SetNextWindowBgAlpha(0.8f);

			ImGuiWindowFlags flags =
				ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
				ImGuiWindowFlags_NoDocking;

			ImGui::Begin(str::format("##%s_win", name), nullptr, flags);
				ImGui::Text(name);
				filledSlider(str::format("##%s", name), &value, minv, maxv, "%.3f", ImGuiSliderFlags_NoInput);
			ImGui::End();
		};


		if (shift) {
			beginMenu("Scale", is_menu_open, scale, 1.f, 5.f);
			findBrush(main_tex);
			return;
		}

		if (ctrl) {
			beginMenu("Depth", is_menu_open, depth, -1.5f, 1.5f);
			findBrush(main_tex);
			return;
		}

		if (alt) {
			beginMenu("Blend amount", is_menu_open, smooth_k, 0.f, 20.f);
			findBrush(main_tex);
			return;
		}

		is_menu_open = shift || ctrl || alt;
	}
}

void BrushEditor::findBrush(Handle<Texture3D> main_tex) {
	if (BrushFindData *data = find_data_handle->map<BrushFindData>()) {
		data->pos = cam_pos;
		data->dir = cam_dir;
		data->depth = depth;
		data->scale = getScale();
		find_data_handle->unmap();
	}

	find_brush->dispatch(1, { find_data_handle }, { main_tex->srv }, { data_handle->uav });
}

void BrushEditor::setState(State newstate) {
	if (state == newstate) return;
	has_changed = true;
	depth = -depth;
	state = newstate;
}

// == PRIVATE FUNCTIONS ========================================

static void centreButton(float width, float padding) {
	const float cursor_pos = ImGui::GetCursorPosX();
	const float column_width = ImGui::GetContentRegionAvail().x;
	const float brush_width = width + padding * 2.f;
	const float offset_x = (column_width - brush_width) * 0.5f;

	ImGui::SetCursorPosX(cursor_pos + offset_x);
}
