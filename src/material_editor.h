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
};

struct MaterialEditor {
	MaterialEditor();
	void drawWidget();
	void update();
	void setOpen(bool is_open);
	bool isOpen() const;

	Handle<Buffer> getBuffer();
	ID3D11ShaderResourceView *getDiffuse();
	ID3D11ShaderResourceView *getBackground();
	ID3D11ShaderResourceView *getIrradiance();

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
	Handle<Buffer> mat_handle = nullptr;

	size_t diffuse_handle;
	size_t background_handle;
	size_t irradiance_handle;

	// widget data
	bool is_open = true;
	bool has_changed = true;
	arr<TexNamePair> textures;
	file::Watcher watcher = "assets/";

	bool should_open_nfd = false;
};