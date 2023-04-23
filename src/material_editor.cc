#include "material_editor.h"

#include <d3d11.h>
#include <imgui.h>

#include "maths.h"
#include "system.h"

MaterialEditor::MaterialEditor() {
	if (!texture.load("assets/testing_texture.png")) {
		fatal("couldn't load default material texture");
	}

	if (!bg_hdr.loadHDR("assets/rainforest_trail_4k.hdr")) {
		fatal("couldn't load default background hdr texture");
	}

	if (!irradiance_map.loadHDR("assets/irradiance_map.hdr")) {
		fatal("couldn't load default irradiance texture");
	}

	mat_handle = Buffer::makeConstant<MaterialPS>(Buffer::Usage::Dynamic);

	if (!mat_handle) {
		gfx::logD3D11messages();
		exit(1);
	}
}

void MaterialEditor::drawWidget() {
	if (!is_open) return;
	if (!ImGui::Begin("Material Editor", &is_open)) {
		ImGui::End();
		return;
	}

	vec2 tex_size = texture.size;
	vec2 clamped = math::clamp(tex_size, vec2(0.f), vec2(200.f));
	clamped.y *= tex_size.y / tex_size.x;

	has_changed |= ImGui::ColorPicker3("Albedo", albedo.data);
	ImGui::Text("Uses texture");

	ImGui::SameLine();
	has_changed |= ImGui::Checkbox("##UseTexture", &has_texture);

	if (ImGui::Button("Load Image From File")) {

	}

	ImGui::BeginDisabled(!has_texture);
		ImGui::Text("Texture");
		ImGui::SameLine();
		const vec2 cursor_pos = ImGui::GetCursorScreenPos();
		ImGui::Image((ImTextureID)texture.srv, clamped);
		if (ImGui::IsItemHovered()) {
			const vec2 mouse_pos = ImGui::GetMousePos();
			const vec2 norm_pos = (mouse_pos - cursor_pos) / clamped;
			const vec2 uv_start = norm_pos - 0.15f;
			const vec2 uv_end = norm_pos + 0.15f;
			ImGui::BeginTooltip();
				ImGui::Image((ImTextureID)texture.srv, clamped * 2.f, uv_start, uv_end);
			ImGui::EndTooltip();
		}
	ImGui::EndDisabled();

	ImGui::End();

	//ImGui::ShowDemoWindow();
}

void MaterialEditor::update() {
	if (!has_changed) return;
	if (Buffer *buf = mat_handle.get()) {
		if (MaterialPS *data = buf->map<MaterialPS>()) {
			data->albedo = albedo;
			data->has_texture = has_texture;
			buf->unmap();
		}
	}
	has_changed = false;
}

ID3D11ShaderResourceView *MaterialEditor::getSRV() {
	return texture.srv;
}