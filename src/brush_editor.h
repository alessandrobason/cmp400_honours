#pragma once

#include "common.h"
#include "vec.h"
#include "texture.h"
#include "handle.h"
#include "utils.h"

struct Buffer;
struct Shader;
struct Camera;

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

GFX_CLASS_CHECK(ShapeData);

enum class Operations : uint32_t {
	None               = 0,
	Union              = 1u << 0,
	Subtraction        = 1u << 1,
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

GFX_CLASS_CHECK(OperationData);

struct BrushData {
	vec3 position;
	float radius;
	vec3 normal;
	float padding__1;
};

GFX_CLASS_CHECK(BrushData);

struct BrushFindData {
	vec3 pos;
	float depth;
	vec3 dir;
	float scale;
};

GFX_CLASS_CHECK(BrushFindData);

struct BrushEditor {
	BrushEditor();
	void drawWidget(Handle<Texture3D> main_tex);
	void update();
	void findBrush(const Camera &cam, Handle<Texture3D> texture);
	void setOpen(bool is_open);
	bool isOpen() const;

	ID3D11UnorderedAccessView *getBrushUAV();
	ID3D11ShaderResourceView *getBrushSRV();
	ID3D11UnorderedAccessView *getDataUAV();
	ID3D11ShaderResourceView *getDataSRV();
	vec3i getBrushSize() const;
	float getScale() const;
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
	void mouseWidget(Handle<Texture3D> main_tex);
	void findBrush(Handle<Texture3D> main_tex);
	void setState(State newstate);

	vec3 position = 0.f;
	float depth = 0.f;
	float smooth_k = 0.5f;
	float scale = 1.f;

	size_t brush_index = 0;
	Handle<Buffer> oper_handle;
	Handle<Buffer> data_handle;
	Handle<Buffer> find_data_handle;
	Handle<Shader> find_brush;

	// fill stuff
	Handle<Buffer> fill_buffer;
	Handle<Shader> fill_shaders[(int)Shapes::Count];

	// widget data
	Handle<Texture2D> brush_icon;
	Handle<Texture2D> eraser_icon;
	Handle<Texture2D> depth_tooltips[3];
	State state = State::Brush;
	bool is_open = true;
	bool has_changed = true;
	vec3 cam_pos;
	vec3 cam_dir;

	arr<TexNamePair> textures;
	bool should_open_nfd = false;
};
