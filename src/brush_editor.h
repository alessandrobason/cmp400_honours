#pragma once

#include "common.h"
#include "vec.h"
#include "texture.h"
#include "handle.h"
#include "utils.h"

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
	uint32_t operation;
	float smooth_k;
	float scale;
	float depth;
};

struct BrushEditor {
	BrushEditor();
	void drawWidget();
	void update();
	void setOpen(bool is_open);
	bool isOpen() const;

	ID3D11UnorderedAccessView *getBrushUAV();
	ID3D11ShaderResourceView *getBrushSRV();
	vec3i getBrushSize() const;
	float getScale() const;
	float getDepth() const;
	Handle<Buffer> getOperHandle();

	enum class State { Brush, Eraser, Count };

private:
	struct TexNamePair {
		TexNamePair(Texture3D &&tex, mem::ptr<char[]> &&name) : tex(mem::move(tex)), name(mem::move(name)) {}
		Texture3D tex;
		mem::ptr<char[]> name;
	};

	Texture3D *get(size_t index);
	const Texture3D *get(size_t index) const;
	size_t addTexture(const char *name);
	size_t checkTextureAlreadyLoaded(const char *name);

	vec3 position = 0.f;
	float depth = 0.f;
	float smooth_k = 0.5f;
	float scale = 1.f;
	bool is_single_click = true;

	//Texture3D texture;
	size_t brush_handle = -1;
	Handle<Buffer> oper_handle;

	// widget data
	Texture2D brush_icon;
	Texture2D eraser_icon;
	State state = State::Brush;
	bool is_open = true;
	bool has_changed = true;

	arr<TexNamePair> textures;
	bool should_open_nfd = false;
};
