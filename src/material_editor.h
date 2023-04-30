#pragma once

#include "vec.h"
#include "utils.h"
#include "buffer.h"
#include "texture.h"

enum class TextureMode : uint32_t {
	None,
	TriplanarBlend,
	SphereCoords,
};

struct MaterialPS {
	vec3 albedo;
	TextureMode texture_mode;
	vec3 specular_colour;
	float smoothness;
	vec3 emissive_colour;
	float specular_probability;
};

struct LightData {
	vec3 light_pos     = vec3(-400, 0, 0);
	float light_radius = 150.f;
	vec3 light_colour  = 1.f;
	unsigned int render_light  = true;
};

struct MaterialEditor {
	MaterialEditor();
	void drawWidget();
	bool update();
	void setOpen(bool is_open);
	bool isOpen() const;

	Handle<Buffer> getBuffer();
	ID3D11ShaderResourceView *getDiffuse();
	ID3D11ShaderResourceView *getBackground();
	ID3D11ShaderResourceView *getIrradiance();

	Handle<Buffer> light_buf;

private:
	struct TexNamePair {
		TexNamePair(Handle<Texture2D> handle, mem::ptr<char[]> &&name) : handle(handle), name(mem::move(name)) {}
		Handle<Texture2D> handle;
		mem::ptr<char[]> name;
	};

	Texture2D *get(size_t index);
	size_t addTexture(const char *name);
	size_t checkTextureAlreadyLoaded(str::view name);

	vec3 albedo = 1;
	TextureMode texture_mode = TextureMode::TriplanarBlend;
	vec3 specular = 1.f;
	float smoothness = 0.f;
	vec3 emissive = 0.f;
	float specular_probability = 0.f;

	LightData light_data;
	float light_strength = 3.f;
	
	Handle<Buffer> mat_handle = nullptr;

	size_t diffuse_handle;
	size_t background_handle;
	size_t irradiance_handle;

	// widget data
	bool is_open = true;
	bool has_changed = true;
	bool is_ray_tracing_dirty = false;
	arr<TexNamePair> textures;
	file::Watcher watcher = "assets/";

	bool should_open_nfd = false;
};