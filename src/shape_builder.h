#pragma once

#include <stdint.h>
#include <vector>

#include "d3d11_fwd.h"
#include "buffer.h"
#include "vec.h"

#if 0
// ordered for minimum padding
struct Shape {
	uint32_t type;
	vec3 centre;
	union ShapeData {
		struct { float radius; } sphere;
		struct { vec3 size; } box;
		struct { vec2 angle_cos_sin; float height; } cone;
		struct { vec3 bottom; float radius; vec3 top; } cylinder;
		struct { float height; } pyramid;
		struct { vec4 data_0; vec3 data_1; } data;
	} as;
	uint32_t parent;
};

static_assert(sizeof(Shape) == (sizeof(vec4) * 3));

enum class ShapeType : uint32_t {
	None,
	Sphere,
	Box,
	Cone,
	Cylinder,
	Pyramid
};

enum class Alteration : uint32_t {
	None,
	Elongated = 1u << 31,
	Rounded = 1u << 30,
	Onioned = 1u << 29,
};

enum class Operations : uint32_t {
	None,
	Smooth = 1 << 28u,
	Union = 1 << 27u,
	Subtraction = 1 << 26u,
	Intersection = 1 << 25u,
};

struct ShapeBuilder {
	ShapeBuilder() = default;
	ShapeBuilder(const ShapeBuilder &) = delete;
	
	void init();
	void cleanup();

	void bind(unsigned int slot = 0);
	void unbind(unsigned int slot = 0);

	void addSphere(const vec3 &centre, float radius, Operations oper = Operations::None, Alteration alter = Alteration::None);
	void addBox(const vec3 &centre, const vec3 &size, Operations oper = Operations::None, Alteration alter = Alteration::None);
	void addCone(const vec3 &centre, const vec2 &angle_cos_sin, float height, Operations oper = Operations::None, Alteration alter = Alteration::None);
	void addCylinder(const vec3 &centre, const vec3 &bottom, const vec3 &top, float radius, Operations oper = Operations::None, Alteration alter = Alteration::None);
	void addPyramid(const vec3 &centre, float height, Operations oper = Operations::None, Alteration alter = Alteration::None);
	//void addShape(vec3 centre, Shape shape, Operations oper = Operations::None, Alteration alter = Alteration::None);

private:
	void destroyBuffer();
	bool makeBuffer();
	void addShape(ShapeType type, Operations oper, Alteration alter, const vec3 &centre, const Shape::ShapeData &data);

	std::vector<Shape> shapes;
	bool dirty = false;
	size_t buffer_cap = 0;
	Buffer buffer;
	dxptr<ID3D11ShaderResourceView> buf_srv = nullptr;
};
#endif

#if 0
struct ShapeType {
	uint32_t shape;
	vec3 centre;
};

enum class Shape : uint32_t {
	None,
	Sphere,
	Box,
	Cone,
	Cylinder,
	Pyramid
};

enum class Alteration : uint32_t {
	None,
	Elongated = 1u << 31,
	Rounded   = 1u << 30,
	Onioned   = 1u << 29,
};

enum class Operations : uint32_t {
	None,
	Smooth       = 1 << 28u,
	Union        = 1 << 27u,
	Subtraction  = 1 << 26u,
	Intersection = 1 << 25u,
};

struct ShapeBuilder {
	void init();
	void cleanup();

	void bind();
	void unbind();

	void addShape(vec3 centre, Shape shape, Operations oper = Operations::None, Alteration alter = Alteration::None);

private:
	void destroyBuffer();
	bool makeBuffer();

	std::vector<ShapeType> shapes;
	bool dirty = false;
	size_t buffer_cap = 0;
	ID3D11Buffer *buffer = nullptr;
	ID3D11ShaderResourceView *buf_srv = nullptr;
};
#endif

#if 0
struct ShapeBuilder {
	void init();
	void cleanup();

	void bind();
	void unbind();

	void pushSphere();

private:
	void pushBytes(const void *bytes, size_t count);
	template<typename T> void pushData(const T &data) { pushBytes(&data, sizeof(T)); }
	void destroyBuffer();
	bool makeBuffer();

	std::vector<uint8_t> byte_stream;
	bool dirty = false;
	size_t buffer_cap = 0;
	ID3D11Buffer *buffer = nullptr;
	ID3D11ShaderResourceView *buf_srv = nullptr;
};
#endif