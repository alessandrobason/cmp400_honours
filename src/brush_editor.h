#pragma once

#include "common.h"
#include "vec.h"
#include "texture.h"
#include "handle.h"
#include "utils.h"

struct Buffer;
struct Shader;

enum class Shapes : int {
	Sphere, Box, Cylinder, None, Count
};

struct ShapeData {
	ShapeData(const vec3 &p = 0, float x = 0, float y = 0, float z = 0, float w = 0);

	vec3 position;
	float padding;
	union {
		struct { float radius;         } sphere;
		struct { vec3 size;            } box;
		struct { float radius, height; } cylinder;
		vec4 data;
	};
};

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

struct BrushData {
	vec3 position;
	float radius;
	vec3 normal;
	float padding__1;
};

struct BrushEditor {
	BrushEditor();
	void drawWidget();
	void update();
	void setOpen(bool is_open);
	bool isOpen() const;

	ID3D11UnorderedAccessView *getBrushUAV();
	ID3D11ShaderResourceView *getBrushSRV();
	ID3D11UnorderedAccessView *getDataUAV();
	ID3D11ShaderResourceView *getDataSRV();
	vec3i getBrushSize() const;
	float getScale() const;
	float getDepth() const;
	Handle<Buffer> getOperHandle();

	void runFillShader(Shapes shape, const ShapeData &data, Handle<Texture3D> destination);

	enum class State { Brush, Eraser, Count };

private:
	struct TexNamePair {
		TexNamePair(Handle<Texture3D> handle, mem::ptr<char[]> &&name) : handle(handle), name(mem::move(name)) {}
		Handle<Texture3D> handle;
		mem::ptr<char[]> name;
	};

	Texture3D *get(size_t index);
	const Texture3D *get(size_t index) const;
	size_t addTexture(const char *name);
	size_t addBrush(const char *name, Shapes shape, const ShapeData &data);
	size_t checkTextureAlreadyLoaded(str::view name);

	vec3 position = 0.f;
	float depth = 0.f;
	float smooth_k = 0.5f;
	float scale = 1.f;
	bool is_single_click = true;

	size_t brush_index = 0;
	Handle<Buffer> oper_handle;
	Handle<Buffer> data_handle;

	// fill stuff
	Handle<Buffer> fill_buffer;
	Handle<Shader> fill_shaders[(int)Shapes::Count];

	// widget data
	Handle<Texture2D> brush_icon;
	Handle<Texture2D> eraser_icon;
	State state = State::Brush;
	bool is_open = true;
	bool has_changed = true;

	arr<TexNamePair> textures;
	bool should_open_nfd = false;
};
