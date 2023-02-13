#pragma once

#include <stdint.h>
#include <vector>

#include "d3d11_fwd.h"
#include "vec.h"

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