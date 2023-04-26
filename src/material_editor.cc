#include "material_editor.h"

#include <d3d11.h>
#include <imgui.h>
#include <nfd.hpp>

#include "maths.h"
#include "system.h"

MaterialEditor::MaterialEditor() {
	diffuse_handle    = addTexture("assets/testing_texture.png");
	background_handle = addTexture("assets/rainforest_trail.png");
	mat_handle = Buffer::makeConstant<MaterialPS>(Buffer::Usage::Dynamic);

	if (diffuse_handle == -1)    fatal("couldn't load default material texture");
	if (background_handle == -1) fatal("couldn't load default background hdr texture");

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

	Texture2D *diffuse = get(diffuse_handle);
	vec2 tex_size = diffuse->size;
	vec2 clamped = math::clamp(tex_size, vec2(0.f), vec2(200.f));
	clamped.y *= tex_size.y / tex_size.x;

	has_changed |= ImGui::ColorPicker3("Albedo", albedo.data);
	ImGui::Text("Uses texture");

	ImGui::SameLine();
	has_changed |= ImGui::Checkbox("##UseTexture", &has_texture);

	if (ImGui::BeginCombo("Textures", textures[diffuse_handle].name.get())) {
		for (size_t i = 0; i < textures.len; ++i) {
			bool is_selected = diffuse_handle == i;
			if (ImGui::Selectable(textures[i].name.get(), is_selected)) {
				diffuse_handle = i;
			}

			if (is_selected) {
				ImGui::SetItemDefaultFocus();
			}
		}

		ImGui::EndCombo();
	}

	if (ImGui::Button("Load Image From File")) {
		should_open_nfd = true;
	}

	ImGui::BeginDisabled(!has_texture);
		ImGui::Text("Texture");
		ImGui::SameLine();
		const vec2 cursor_pos = ImGui::GetCursorScreenPos();
		ImGui::Image((ImTextureID)diffuse->srv, clamped);
		if (ImGui::IsItemHovered()) {
			const vec2 mouse_pos = ImGui::GetMousePos();
			const vec2 norm_pos = (mouse_pos - cursor_pos) / clamped;
			const vec2 uv_start = norm_pos - 0.15f;
			const vec2 uv_end = norm_pos + 0.15f;
			ImGui::BeginTooltip();
				ImGui::Image((ImTextureID)diffuse->srv, clamped * 2.f, uv_start, uv_end);
			ImGui::EndTooltip();
		}
	ImGui::EndDisabled();

	ImGui::End();
}

void MaterialEditor::update() {
	if (should_open_nfd) {
		should_open_nfd = false;

		NFD::UniquePathU8 path;
		nfdu8filteritem_t filter[] = { { "Normal Image", "jpg,jpeg,png,bmp,psd,tga,gif,pic" }, { "HDR Image", "hdr" } };
		nfdresult_t result = NFD::OpenDialog(path, filter, ARRLEN(filter), "assets");
		if (result == NFD_OKAY) {
			if (str::cmp("hdr", file::getExtension(path.get()))) {
				diffuse_handle = addTextureHDR(path.get());
			}
			else {
				diffuse_handle = addTexture(path.get());
			}
		}
		else if (result == NFD_CANCEL) {
			// user has closed the dialog without selecting
		}
		else {
			err("error with the file dialog: %s", NFD::GetError());
		}
	}

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

void MaterialEditor::setOpen(bool new_is_open) {
	is_open = new_is_open;
}

bool MaterialEditor::isOpen() const {
	return is_open;
}

Handle<Buffer> MaterialEditor::getBuffer() {
	return mat_handle;
}

ID3D11ShaderResourceView *MaterialEditor::getDiffuse() {
	if (Texture2D *tex = get(diffuse_handle)) return tex->srv;
	return nullptr;
}

ID3D11ShaderResourceView *MaterialEditor::getBackground() {
	if (Texture2D *tex = get(background_handle)) return tex->srv;
	return nullptr;
}

ID3D11ShaderResourceView *MaterialEditor::getIrradiance() {
	if (Texture2D *tex = get(irradiance_handle)) return tex->srv;
	return nullptr;
}

Texture2D *MaterialEditor::get(size_t index) {
	return index < textures.len ? textures[index].handle.get() : nullptr;
}

size_t MaterialEditor::addTexture(const char *path) {
	mem::ptr<char[]> name = file::getFilename(path);
	size_t index = checkTextureAlreadyLoaded(name.get());
	if (index != -1) {
		warn("texture %s is already loaded", name.get());
		return index;
	}

	Handle<Texture2D> newtex = Texture2D::load(path);
	if (!newtex) {
		err("couldn't load texture %s", name.get());
		return -1;
	}

	index = textures.len;
	textures.push(newtex, mem::move(name));
	return index;
}

size_t MaterialEditor::addTextureHDR(const char *path) {
	mem::ptr<char[]> name = file::getFilename(path);
	size_t index = checkTextureAlreadyLoaded(name.get());
	if (index != -1) {
		warn("texture %s is already loaded", name.get());
		return index;
	}

	Handle<Texture2D> newtex = Texture2D::loadHDR(path);
	if (!newtex) {
		err("couldn't load HDR texture %s", name.get());
		return -1;
	}

	index = textures.len;
	textures.push(newtex, mem::move(name));
	return index;
}

size_t MaterialEditor::checkTextureAlreadyLoaded(const char *name) {
	for (size_t i = 0; i < textures.len; ++i) {
		if (str::cmp(name, textures[i].name.get())) {
			return i;
		}
	}
	return -1;
}