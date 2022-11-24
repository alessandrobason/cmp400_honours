#pragma once

#include <stdint.h>

#include "vec.h"
#include "colours.h"
#include "slice.h"
#include "d3d11_fwd.h"

struct Vertex {
	vec3 position;
	Colour colour;
};

using Index = uint16_t;

struct Mesh {
	Mesh() = default;
	Mesh(const Mesh &m) = delete;
	Mesh(Mesh &&m);
	~Mesh();

	Mesh &operator=(Mesh &&m);

	bool create(Slice<Vertex> vertices, Slice<Index> indices);
	void cleanup();

	void render();

	int vertex_count = 0;
	int index_count = 0;
	ID3D11Buffer *vert_buf = nullptr;
	ID3D11Buffer *ind_buf = nullptr;
};
