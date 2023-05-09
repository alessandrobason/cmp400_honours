#pragma once

#include "vec.h"
#include "buffer.h"
#include "texture.h"
#include "mem.h"
#include "str.h"
#include "arr.h"

struct MaterialPS {
	vec3 albedo;
	uint use_texture;
	vec3 specular_colour;
	float smoothness;
	vec3 emissive_colour;
	float specular_probability;
};

GFX_CLASS_CHECK(MaterialPS);

struct LightData {
	LightData() = default;
	LightData(const vec3 &p, float r, const vec3 &c, bool rl) : pos(p), radius(r), colour(c), render(rl) {}
	vec3 pos;
	float radius;
	vec3 colour;
	uint render;
};

GFX_CLASS_CHECK(LightData);

struct MaterialEditor {
	MaterialEditor();
	void drawWidget();
	bool update();
	void setOpen(bool is_open);
	bool isOpen() const;

	bool useTonemapping() const;
	float getExposure() const;
	Handle<Buffer> getBuffer() const;
	Handle<Buffer> getLights() const;
	size_t getLightsCount() const;
	ID3D11ShaderResourceView *getDiffuse();
	ID3D11ShaderResourceView *getBackground();

private:
	struct TexNamePair {
		TexNamePair(Handle<Texture2D> handle, mem::ptr<char[]> &&name) : handle(handle), name(mem::move(name)) {}
		Handle<Texture2D> handle;
		mem::ptr<char[]> name;
	};

	struct AsyncTex {
		using Promise = thr::Promise<Handle<Texture2D>>;
		mem::ptr<Promise> promise;
		mem::ptr<char[]> name;
		size_t *handle = nullptr;
	};

	Texture2D *get(size_t index);
	size_t addTexture(const char *name);
	void addTextureAsync(const char *name, size_t *handle = nullptr);
	size_t checkTextureAlreadyLoaded(str::view name);
	void addLight(const vec3 &pos, float radius, float strength = 3, const vec3 &colour = 1, bool render = true);
	void updateLightsBuffer();

	bool texChooser(const char *label, size_t &handle, const char *tip = nullptr);

	vec3 albedo = 1;
	bool use_texture = true;
	vec3 specular = 1.f;
	float smoothness = 0.f;
	vec3 emissive = 0.f;
	float specular_probability = 0.f;
	bool use_tonemapping = true;
	float exposure_bias = 2.f;

	arr<LightData> lights;
	arr<float> light_strengths;
	Handle<Buffer> lights_handle;
	// start with 10 lights so we don't need to allocate more for a while
	size_t cur_lights_count = 10;
	
	Handle<Buffer> mat_handle = nullptr;

	size_t diffuse_handle;
	size_t background_handle;

	// widget data
	bool is_open = true;
	bool material_dirty = true;
	bool light_dirty = true;
	bool ray_tracing_dirty = false;
	arr<TexNamePair> textures;
	arr<AsyncTex> async_textures;

	bool should_open_nfd = false;
};