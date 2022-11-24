#include "shader.h"

#include <d3d11.h>

#include "system.h"
#include "tracelog.h"
#include "file_utils.h"
#include "str_utils.h"
#include "macros.h"
#include "mesh.h"

Shader::Shader(Shader &&s) {
	*this = std::move(s);
}

Shader::~Shader() {
	cleanup();
}

Shader &Shader::operator=(Shader &&s) {
	if (this != &s) {
		vert = s.vert;
		pixel = s.pixel;
		layout = s.layout;
		//matbuf = std::move(s.matbuf);

		s.vert = nullptr;
		s.pixel = nullptr;
		s.layout = nullptr;
	}
	return *this;
}

bool Shader::create(const char *vertex_file, const char *pixel_file) {
	size_t vert_len, pixel_len;
	std::unique_ptr<uint8_t[]> vert_buf
		= fs::readWhole(str::format("shaders/bin/%s.cso", vertex_file).get(), vert_len);
	std::unique_ptr<uint8_t[]> pixel_buf 
		= fs::readWhole(str::format("shaders/bin/%s.cso", pixel_file).get(), pixel_len);
	
	if (!vert_buf || !vert_len) {
		err("couldn't read vertex shader file");
		return false;
	}

	if (!pixel_buf || !pixel_len) {
		err("couldn't read pixel shader file");
		return false;
	}

	HRESULT hr = gfx::device->CreateVertexShader(vert_buf.get(), vert_len, nullptr, &vert);
	if (FAILED(hr)) {
		err("couldn't create vertex shader");
		return false;
	}

	hr = gfx::device->CreatePixelShader(pixel_buf.get(), pixel_len, nullptr, &pixel);
	if (FAILED(hr)) {
		err("couldn't create pixel shader");
		return false;
	}

	D3D11_INPUT_ELEMENT_DESC layout_desc[] = {
		"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, (UINT)offsetof(Vertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0,
		"COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT,  0, (UINT)offsetof(Vertex, colour),   D3D11_INPUT_PER_VERTEX_DATA, 0,
	};

	hr = gfx::device->CreateInputLayout(
		layout_desc,    ARRLEN(layout_desc), 
		vert_buf.get(), vert_len, 
		&layout
	);
	if (FAILED(hr)) {
		err("couldn't create input layout");
		return false;
	}

	//if (!matbuf.create<MatrixBuffer>(Usage::Dynamic)) {
	//	err("couldn't create the matrix buffer");
	//	return false;
	//}

	return true;
}

void Shader::cleanup() {
	SAFE_RELEASE(vert);
	SAFE_RELEASE(pixel);
	SAFE_RELEASE(layout);
	//matbuf.cleanup();
}

/*
bool Shader::update(const matrix &world, const matrix &view, const matrix &proj) {
	MatrixBuffer *buf = matbuf.map<MatrixBuffer>();
	if (!buf) {
		err("could not map subresource matrix buffer");
		return false;
	}

	buf->world = world;
	buf->view = view;
	buf->proj = proj;

	matbuf.unmapVS();

	return true;
}
*/

void Shader::bind() {
	gfx::context->IASetInputLayout(layout);
	gfx::context->VSSetShader(vert, nullptr, 0);
	gfx::context->PSSetShader(pixel, nullptr, 0);
}

void Shader::unbind() {

}
