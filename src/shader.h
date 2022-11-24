#pragma once

#include "d3d11_fwd.h"
#include "matrix.h"
#include "buffer.h"

struct MatrixBuffer {
	matrix world, view, proj;
};

struct Shader {
	Shader() = default;
	Shader(const Shader &s) = delete;
	Shader(Shader &&s);
	~Shader();

	Shader &operator=(Shader &&s);

	bool create(const char *vertex_file, const char *pixel_file);
	void cleanup();

	bool update(const matrix &world, const matrix &view, const matrix &proj);
	void bind();

	ID3D11VertexShader *vert = nullptr;
	ID3D11PixelShader *pixel = nullptr;
	ID3D11InputLayout *layout = nullptr;
	Buffer matbuf;
};