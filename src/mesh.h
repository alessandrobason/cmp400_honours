#pragma once

#include "gfx_common.h"
#include "vec.h"
#include "slice.h"

struct Mesh {
	struct Vertex {
		vec3 position;
		vec2 uv;
	};

	// check that the Vertex struct does not have any padding
	static_assert(sizeof(Vertex) == (sizeof(float) * 3 + sizeof(float) * 2));

	// we're doing everything in shaders, this is not that important
	using Index = uint16_t;

	Mesh() = default;
	Mesh(const Mesh &m) = delete;
	Mesh(Mesh &&m);
	~Mesh();

	Mesh &operator=(Mesh &&m);

	bool create(Slice<Vertex> vertices, Slice<Index> indices);
	void cleanup();

	void render();

	int index_count = 0;
	dxptr<ID3D11Buffer> vert_buf = nullptr;
	dxptr<ID3D11Buffer> ind_buf = nullptr;
};