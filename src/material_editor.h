#pragma once

#include "vec.h"
#include "buffer.h"
#include "texture.h"

struct MaterialPS {
	vec3 albedo;
	int has_texture;
};

struct MaterialEditor {
	MaterialEditor();
	void drawWidget();
	void update();

	ID3D11ShaderResourceView *getSRV();

	vec3 albedo = 1;
	bool has_texture = true;
	Handle<Buffer> mat_handle = nullptr;
	Texture2D texture;
	Texture2D bg_hdr;
	Texture2D irradiance_map;

	// widget data
	bool is_open = true;
	bool has_changed = true;
};