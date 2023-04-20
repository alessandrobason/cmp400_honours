#pragma once

#include <stdint.h>
#include "vec.h"
#include "gfx.h"

enum class Operations : uint32_t {
	None               = 0,
	Union              = 1u << 1,
	Subtraction        = 1u << 2,
	Smooth             = 1u << 31,
	SmoothUnion        = Smooth | Union,
	SmoothSubtraction  = Smooth | Subtraction,
};

// Used in the sculpting shader
struct OperationData {
	Operations operation;
	float smooth_k;
	float scale;
	float depth;
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

	Texture3D texture;
	//Buffer *buffer = nullptr;
	Handle<Buffer> buf_handle;

	// widget data
	enum class State { Brush, Eraser, Count };
	
	Texture2D brush_icon;
	Texture2D eraser_icon;
	State state = State::Brush;
	bool is_open = true;
	bool has_changed = false;
};
