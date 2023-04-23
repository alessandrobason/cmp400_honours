#pragma once

#include "common.h"
#include "vec.h"
#include "texture.h"
#include "handle.h"

struct Buffer;

enum class Operations : uint32_t {
	None               = 0,
	Subtraction        = 1u << 29,
	Union              = 1u << 30,
	Smooth             = 1u << 31,
	SmoothUnion        = Smooth | Union,
	SmoothSubtraction  = Smooth | Subtraction,
};

// Used in the sculpting shader
struct OperationData {
	//Operations operation;
	uint32_t operation;
	float smooth_k;
	float scale;
	float depth;
	vec3 colour;
	float padding__0;
};

struct Brush {
	Brush();
	void setBuffer(Handle<Buffer> handle);
	void drawWidget();
	void update();

	ID3D11UnorderedAccessView *getUAV();
	ID3D11ShaderResourceView *getSRV();

	vec3 position = 0.f;
	float depth = 0.f;
	float smooth_k = 0.5f;
	float scale = 1.f;
	bool is_single_click = true;
	uint8_t material_index = 1;

	Texture3D texture;
	Handle<Buffer> buf_handle;

	// widget data
	enum class State { Brush, Eraser, Count };
	
	Texture2D brush_icon;
	Texture2D eraser_icon;
	State state = State::Brush;
	bool is_open = true;
	bool has_changed = true;
};
