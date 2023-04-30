#include "material_editor.h"

#include <d3d11.h>
#include <imgui.h>
#include <nfd.hpp>

#include "maths.h"
#include "system.h"
#include "widgets.h"

MaterialEditor::MaterialEditor() {
	diffuse_handle    = addTexture("assets/testing_texture.png");
	background_handle = addTexture("assets/rainforest_trail.png");
	mat_handle = Buffer::makeConstant<MaterialPS>(Buffer::Usage::Dynamic);
	light_buf  = Buffer::makeConstant<LightData>(Buffer::Usage::Dynamic);

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

	is_ray_tracing_dirty = false;

	separatorText("Material");

	has_changed |= ImGui::ColorPicker3("Albedo", albedo.data);
	has_changed |= ImGui::ColorPicker3("Specular colour", specular.data);
	has_changed |= ImGui::ColorPicker3("Emissive colour", emissive.data);
	has_changed |= ImGui::SliderFloat("Smoothness", &smoothness, 0.f, 1.f);
	has_changed |= ImGui::SliderFloat("Specular probability", &specular_probability, 0.f, 1.f);

	separatorText("Light");

	bool render_light = light_data.render_light;

	has_changed |= ImGui::SliderFloat3("Position", light_data.light_pos.data, -1000, 1000);
	has_changed |= ImGui::ColorPicker3("Colour", light_data.light_colour.data);
	has_changed |= ImGui::SliderFloat("Strength", &light_strength, 1.f, 100.f);
	has_changed |= ImGui::SliderFloat("Radius", &light_data.light_radius, 0.1f, 1000);
	has_changed |= ImGui::Checkbox("Render", &render_light);

	light_data.render_light = render_light;
	
	separatorText("Textures");
	
	ImGui::Text("Uses texture");

	ImGui::SameLine();
	has_changed |= ImGui::Combo("##TexMode", (int *)&texture_mode, "No Texture\0Default\0Spherical");

	const auto &texChooser = [](const char *label, size_t &handle, Slice<TexNamePair> textures, bool &is_dirty) {
		if (ImGui::BeginCombo(label, textures[handle].name.get())) {
			for (size_t i = 0; i < textures.len; ++i) {
				bool is_selected = handle == i;
				if (ImGui::Selectable(textures[i].name.get(), is_selected)) {
					handle = i;
					is_dirty = true;
				}

				if (is_selected) {
					ImGui::SetItemDefaultFocus();
				}
			}

			ImGui::EndCombo();
		}
	};

	const auto showTexture = [](const char *label, Texture2D *tex, const vec2 &max_size) {
		if (!tex) return;
		vec2 tex_size = tex->size;
		vec2 clamped = math::clamp(tex_size, vec2(0.f), max_size);
		clamped.y *= tex_size.y / tex_size.x;

		ImGui::Text(label);
		ImGui::SameLine();
		const vec2 cursor_pos = ImGui::GetCursorScreenPos();
		ImGui::Image((ImTextureID)tex->srv, clamped);
		if (ImGui::IsItemHovered()) {
			const vec2 mouse_pos = ImGui::GetMousePos();
			const vec2 norm_pos = (mouse_pos - cursor_pos) / clamped;
			const vec2 uv_start = norm_pos - 0.15f;
			const vec2 uv_end = norm_pos + 0.15f;
			ImGui::BeginTooltip();
			ImGui::Image((ImTextureID)tex->srv, clamped * 2.f, uv_start, uv_end);
			ImGui::EndTooltip();
		}
	};

	texChooser("Diffuse", diffuse_handle, textures, is_ray_tracing_dirty);
	texChooser("Background", background_handle, textures, is_ray_tracing_dirty);

	if (ImGui::Button("Load Image From File")) {
		should_open_nfd = true;
	}

	ImGui::BeginDisabled(texture_mode == TextureMode::None);
		showTexture("Diffuse", get(diffuse_handle), vec2(200.f));
	ImGui::EndDisabled();

	showTexture("Background", get(background_handle), vec2(200.f));

	ImGui::End();
}

bool MaterialEditor::update() {
	watcher.update();
	auto changed = watcher.getChangedFiles();
	while (changed) {
		str::view name = file::getFilename(changed->name.get());

		for (TexNamePair &tex : textures) {
			if (name == tex.name.get()) {
				if (tex.handle->loadFromFile(str::format("%s/%s", watcher.getWatcherDir(), changed->name.get()))) {
					info("reloaded texture %s", changed->name.get());
				}
				break;
			}
		}

		changed = watcher.getChangedFiles(changed);
	}

	if (should_open_nfd) {
		should_open_nfd = false;

		NFD::UniquePathU8 path;
		nfdu8filteritem_t filter[] = { { "Normal Image", "jpg,jpeg,png,bmp,psd,tga,gif,pic" }, { "HDR Image", "hdr" } };
		nfdresult_t result = NFD::OpenDialog(path, filter, ARRLEN(filter), "assets");
		if (result == NFD_OKAY) {
			addTexture(path.get());
		}
		else if (result == NFD_CANCEL) {
			// user has closed the dialog without selecting
		}
		else {
			err("error with the file dialog: %s", NFD::GetError());
		}
	}

	if (!has_changed) return is_ray_tracing_dirty;
	if (Buffer *buf = mat_handle.get()) {
		if (MaterialPS *data = buf->map<MaterialPS>()) {
			data->albedo               = albedo;
			data->texture_mode         = texture_mode;
			data->specular_colour      = specular;
			data->smoothness           = smoothness;
			data->emissive_colour      = emissive;
			data->specular_probability = specular_probability;
			buf->unmap();
		}
	}
	if (LightData *data = light_buf->map<LightData>()) {
		*data = light_data;
		data->light_colour *= light_strength;
		light_buf->unmap();
	}
	has_changed = false;
	return true;
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

static bool hasChanged(const char *filename) {

}

Texture2D *MaterialEditor::get(size_t index) {
	return index < textures.len ? textures[index].handle.get() : nullptr;
}

size_t MaterialEditor::addTexture(const char *path) {
	str::view name = file::getFilename(path);
	size_t index = checkTextureAlreadyLoaded(name);
	if (index != -1) {
		warn("texture %s is already loaded", name.data);
		return index;
	}

	Handle<Texture2D> newtex = Texture2D::load(path);
	if (!newtex) {
		err("couldn't load texture %s", name.data);
		return -1;
	}

	index = textures.len;
	textures.push(newtex, name.dup());
	watcher.watchFile(file::getNameAndExt(path));
	return index;
}

size_t MaterialEditor::checkTextureAlreadyLoaded(str::view name) {
	for (size_t i = 0; i < textures.len; ++i) {
		if (name == textures[i].name.get()) {
			return i;
		}
	}
	return -1;
}
