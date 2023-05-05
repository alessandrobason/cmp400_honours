#include "material_editor.h"

#include <d3d11.h>
#include <imgui.h>
#include <nfd.hpp>

#include "maths.h"
#include "system.h"
#include "widgets.h"

MaterialEditor::MaterialEditor() {
	diffuse_handle    = addTexture("assets/ground texture.png");
	background_handle = addTexture("assets/rainforest_trail_4k.hdr");
	mat_handle        = Buffer::makeConstant<MaterialPS>(Buffer::Usage::Dynamic);
	lights_handle     = Buffer::makeStructured<LightData>(cur_lights_count, Bind::GpuRead | Bind::CpuWrite);

	if (diffuse_handle == -1)    fatal("couldn't load default material texture");
	if (background_handle == -1) fatal("couldn't load default background hdr texture");

	if (!lights_handle) gfx::errorExit();
	if (!mat_handle)    gfx::errorExit();

	addLight(vec3(-400, 0, 0), 150.f);

	assert(cur_lights_count >= lights.len);

	if (LightData *data = lights_handle->map<LightData>()) {
		for (size_t i = 0; i < lights.len; ++i) {
			data[i] = lights[i];
			data[i].colour *= light_strengths[i];
		}
		lights_handle->unmap();
	}
}

void MaterialEditor::drawWidget() {
	if (!is_open) return;
	if (!ImGui::Begin("Material Editor", &is_open)) {
		ImGui::End();
		return;
	}

	ray_tracing_dirty = false;

	const auto colourEdit = [](const char *label, vec3 &colour, const char *tip = nullptr) -> bool {
		static ImGuiColorEditFlags colour_flags = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel;
		ImGui::Text(label);
		if (tip) tooltip(tip);
		ImGui::SameLine();
		return ImGui::ColorEdit3(label, colour.data, colour_flags);
	};

	const auto slider = [](const char *label, float &value, float vmin = 0, float vmax = 1) -> bool {
		ImGui::Text(label);
		return filledSlider(str::format("##%s", label), &value, vmin, vmax);
	};

	const auto label = [](const char *str, const char *tip = nullptr) {
		ImGui::Text(str);
		if (tip) tooltip(tip);
		ImGui::SameLine();
	};

	const auto &texChooser = [](const char *label, size_t &handle, Slice<TexNamePair> textures, const char *tip = nullptr) -> bool {
		bool has_changed = false;
		ImGui::Text(label);
		if (tip) tooltip(tip);

		if (ImGui::BeginCombo(label, textures[handle].name.get())) {
			for (size_t i = 0; i < textures.len; ++i) {
				bool is_selected = handle == i;
				if (ImGui::Selectable(textures[i].name.get(), is_selected)) {
					handle = i;
					has_changed = true;
				}

				if (is_selected) {
					ImGui::SetItemDefaultFocus();
				}
			}

			ImGui::EndCombo();
		}
		return has_changed;
	};

	const auto showTexture = [](const char *label, Texture2D *tex, const vec2 &max_size) {
		if (!tex) return;
		vec2 tex_size = tex->size;
		vec2 clamped = math::clamp(tex_size, vec2(0.f), max_size);
		clamped.y *= tex_size.y / tex_size.x;

		ImGui::Text(label);
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

	ImGui::PushItemWidth(-1);
		separatorText("Material");

		label(
			"Use tonemapping", 
			"Apply a \"filter\" on the final image, it can be very useful "
			"if you're using an HDR (high dynamic range) image as your background "
			"and the contrast is too strong"
		);
		ray_tracing_dirty |= ImGui::Checkbox("##tonemapping", &use_tonemapping);

		ImGui::BeginDisabled(!use_tonemapping);
		label("Exposure");
		ray_tracing_dirty |= ImGui::DragFloat("##exposure", &exposure_bias, 0.5f, 0.01f, 10.f);
		ImGui::EndDisabled();

		material_dirty |= colourEdit("Colour", albedo);
		material_dirty |= colourEdit("Reflection colour", specular);
		material_dirty |= colourEdit("Emissive colour", emissive, "The colour that the material emits, think of it as if the material was a light and this was its colour");

		material_dirty |= slider("Smoothness", smoothness);
		material_dirty |= slider("Shininess", specular_probability);

		separatorText("Lights");
		
		for (size_t i = 0; i < lights.len; ++i) {
			LightData &light = lights[i];

			ImGui::PushID((int)i);
			if (ImGui::CollapsingHeader(str::format("Light #%zu", i + 1))) {
				bool render_light = light.render;

				if (ImGui::Button("Remove")) {
					light_dirty = true;
					lights.removeSlow(i--);
					ImGui::PopID();
					continue;
				}

				ImGui::Text("Position");
				light_dirty |= ImGui::DragFloat3("##Position", light.pos.data);
				light_dirty |= colourEdit("Colour", light.colour);
				light_dirty |= slider("Strength", light_strengths[i], 1.f, 100.f);
				light_dirty |= slider("Radius", light.radius, 0.1f, 1000);
				label("Visible");
				light_dirty |= ImGui::Checkbox("##Render", &render_light);
				ImGui::Separator();

				light.render = render_light;
			}
			ImGui::PopID();
		}

		if (ImGui::Button("Add Light")) {
			addLight(200, 100);
			light_dirty = true;
		}

		separatorText("Textures");
	
		ImGui::Text("Use Texture");
		tooltip("If this is turned off, only the base colour is used for the material");
		ImGui::SameLine();
		material_dirty |= ImGui::Checkbox("##UseTex", &use_texture);

		ray_tracing_dirty |= texChooser("Diffuse", diffuse_handle, textures, "The texture applied on the sculpture");
		ray_tracing_dirty |= texChooser("Background", background_handle, textures);

		if (ImGui::Button("Load Image From File")) {
			should_open_nfd = true;
		}

		ImGui::BeginDisabled(!use_texture);
			showTexture("Diffuse", get(diffuse_handle), vec2(200.f));
		ImGui::EndDisabled();

		showTexture("Background", get(background_handle), vec2(200.f));
	ImGui::PopItemWidth();

	ImGui::End();
}

bool MaterialEditor::update() {
	watcher.update();
	auto changed = watcher.getChangedFiles();
	while (changed) {
		ray_tracing_dirty = true;
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

		nfdu8filteritem_t filter[] = { { "Image", "jpg,jpeg,png,bmp,psd,tga,gif,pic,hdr" } };
		NFD::UniquePathSet paths;
	 	nfdresult_t result = NFD::OpenDialogMultiple(paths, filter, ARRLEN(filter), "assets");
		if (result == NFD_OKAY) {
			nfdpathsetsize_t len = 0;
			result = NFD::PathSet::Count(paths, len);
			if (result == NFD_OKAY) {
				for (nfdpathsetsize_t i = 0; i < len; ++i) {
					NFD::UniquePathSetPathU8 path;
					result = NFD::PathSet::GetPath(paths, i, path);
					if (result == NFD_OKAY) {
						addTexture(path.get());
					}
					else {
						err("couldn't get nfd path from path set");
					}
				}
			}
			else {
				err("couldn't get nfd path set count");
			}
		}
		else if (result == NFD_CANCEL) {
			// user has closed the dialog without selecting
		}
		else {
			err("error with the file dialog: %s", NFD::GetError());
		}
	}

	bool has_changed = material_dirty || light_dirty || ray_tracing_dirty;

	if (material_dirty) {
		material_dirty = false;
		if (MaterialPS *data = mat_handle->map<MaterialPS>()) {
			data->albedo = albedo;
			data->use_texture = (uint)use_texture;
			data->specular_colour = specular;
			data->smoothness = smoothness;
			data->emissive_colour = emissive;
			data->specular_probability = specular_probability;
			mat_handle->unmap();
		}
	}

	if (light_dirty) {
		light_dirty = false;
		updateLightsBuffer();
	}

	return has_changed;
}

void MaterialEditor::setOpen(bool new_is_open) {
	is_open = new_is_open;
}

bool MaterialEditor::isOpen() const {
	return is_open;
}

bool MaterialEditor::useTonemapping() const {
	return use_tonemapping;
}

float MaterialEditor::getExposure() const {
	return exposure_bias;
}

Handle<Buffer> MaterialEditor::getBuffer() const {
	return mat_handle;
}

Handle<Buffer> MaterialEditor::getLights() const {
	return lights_handle;
}

size_t MaterialEditor::getLightsCount() const {
	return lights.len;
}

ID3D11ShaderResourceView *MaterialEditor::getDiffuse() {
	if (Texture2D *tex = get(diffuse_handle)) return tex->srv;
	return nullptr;
}

ID3D11ShaderResourceView *MaterialEditor::getBackground() {
	if (Texture2D *tex = get(background_handle)) return tex->srv;
	return nullptr;
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

void MaterialEditor::addLight(const vec3 &pos, float radius, float strength, const vec3 &colour, bool render) {
	lights.push(pos, radius, colour, render);
	light_strengths.push(strength);
}

void MaterialEditor::updateLightsBuffer() {
	if (cur_lights_count < lights.len) {
		cur_lights_count = lights.len * 2;
		lights_handle->resize(cur_lights_count);
	}

	if (LightData *data = lights_handle->map<LightData>()) {
		for (size_t i = 0; i < lights.len; ++i) {
			data[i] = lights[i];
			data[i].colour *= light_strengths[i];
		}
		lights_handle->unmap();
	}
}