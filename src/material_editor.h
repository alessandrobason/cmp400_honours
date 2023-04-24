#pragma once

#include "vec.h"
#include "utils.h"
#include "buffer.h"
#include "texture.h"

struct MaterialPS {
	vec3 albedo;
	int has_texture;
};

struct TexNamePair {
	TexNamePair(Texture2D &&tex, mem::ptr<char[]> &&name) : tex(mem::move(tex)), name(mem::move(name)) {}
	Texture2D tex;
	mem::ptr<char[]> name;
};

struct MaterialEditor {
	MaterialEditor();
	void drawWidget();
	void update();

	Handle<Buffer> getBuffer();
	ID3D11ShaderResourceView *getDiffuse();
	ID3D11ShaderResourceView *getBackground();
	ID3D11ShaderResourceView *getIrradiance();

private:
	Texture2D *get(size_t index);
	size_t addTexture(const char *name);
	size_t addTextureHDR(const char *name);
	size_t checkTextureAlreadyLoaded(const char *name);

	vec3 albedo = 1;
	bool has_texture = true;
	Handle<Buffer> mat_handle = nullptr;

	size_t diffuse_handle = -1;
	size_t background_handle = -1;
	size_t irradiance_handle = -1;

	// widget data
	bool is_open = true;
	bool has_changed = true;
	arr<TexNamePair> textures;

	bool should_open_nfd = false;
};