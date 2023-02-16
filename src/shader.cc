#include "shader.h"

#include <d3d11.h>

#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler")

#include "system.h"
#include "tracelog.h"
#include "utils.h"
#include "macros.h"
#include "mesh.h"

static dxptr<ID3DBlob> compileShader(const char *filename, ShaderType type);

Shader::Shader(Shader &&s) {
	*this = std::move(s);
}

Shader::~Shader() {
	cleanup();
}

Shader &Shader::operator=(Shader &&s) {
	if (this != &s) {
		std::swap(vert_sh, s.vert_sh);
		std::swap(pixel_sh, s.pixel_sh);
		std::swap(compute_sh, s.compute_sh);
		std::swap(layout, s.layout);
		std::swap(buffers, s.buffers);
	}
	return *this;
}

bool Shader::loadVertex(const char *filename) {
	vert_sh.destroy();

	file::MemoryBuf buf = file::read(str::format("shaders/bin/%s.cso", filename));

	if (!buf.data) {
		err("couldn't read vertex shader file %s", filename);
		return false;
	}

	return loadVertex(buf.data.get(), buf.size);
}

bool Shader::loadVertex(const void *data, size_t len) {
	HRESULT hr = gfx::device->CreateVertexShader(data, len, nullptr, &vert_sh);
	if (FAILED(hr)) {
		err("couldn't create vertex shader");
		return false;
	}

	// no need to recreate the layout if its already loaded
	if (layout) {
		return true;
	}

	D3D11_INPUT_ELEMENT_DESC layout_desc[] = {
		"POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0, (UINT)offsetof(Vertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0,
		"TEXCOORDS", 0, DXGI_FORMAT_R32G32_FLOAT,       0, (UINT)offsetof(Vertex, uv),       D3D11_INPUT_PER_VERTEX_DATA, 0,
	};

	hr = gfx::device->CreateInputLayout(
		layout_desc, ARRLEN(layout_desc),
		data, len,
		&layout
	);
	if (FAILED(hr)) {
		err("couldn't create input layout");
		return false;
	}

	return true;
}

bool Shader::loadFragment(const char *filename) {
	pixel_sh.destroy();

	file::MemoryBuf buf = file::read(str::format("shaders/bin/%s.cso", filename));

	if (!buf.data) {
		err("couldn't read fragment shader file %s", filename);
		return false;
	}

	return loadFragment(buf.data.get(), buf.size);
}

bool Shader::loadFragment(const void *data, size_t len) {
	HRESULT hr = gfx::device->CreatePixelShader(data, len, nullptr, &pixel_sh);
	if (FAILED(hr)) {
		err("couldn't create fragment shader");
		return false;
	}

	return true;
}

bool Shader::loadCompute(const char *filename) {
	compute_sh.destroy();

	file::MemoryBuf buf = file::read(str::format("shaders/bin/%s.cso", filename));

	if (!buf.data) {
		err("couldn't read compute shader file %s", filename);
		return false;
	}

	return loadCompute(buf.data.get(), buf.size);
}

bool Shader::loadCompute(const void *data, size_t len) {
	HRESULT hr = gfx::device->CreateComputeShader(data, len, nullptr, &compute_sh);
	if (FAILED(hr)) {
		err("couldn't create compute shader");
		return false;
	}

	return true;
}

bool Shader::load(const char *vertex, const char *fragment, const char *compute) {
	bool success = true;
	if (vertex)   success &= loadVertex(vertex);
	if (fragment) success &= loadFragment(fragment);
	if (compute)  success &= loadCompute(compute);
	return success;
}

int Shader::addBuffer(size_t type_size, Usage usage, bool can_write, bool can_read) {
	Buffer newbuf;
	if (!newbuf.create(type_size, usage, can_write, can_read)) {
		err("couldn't create new buffer!");
		return -1;
	}
	int index = (int)buffers.size();
	buffers.emplace_back(std::move(newbuf));
	return index;
}

Buffer *Shader::getBuffer(int index) {
	if (index < 0 || index >= (int)buffers.size()) {
		return nullptr;
	}
	return &buffers[index];
}

bool Shader::addSampler() {
	D3D11_SAMPLER_DESC desc;
	mem::zero(desc);
	desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	float border[4] = { 100000, 100000, 100000, 100000 };
	mem::copy(desc.BorderColor, border);
	desc.ComparisonFunc = D3D11_COMPARISON_NEVER;

	HRESULT hr = gfx::device->CreateSamplerState(&desc, &sampler);
	if (FAILED(hr)) {
		err("couldn't create sampler state");
		return false;
	}

	return true;
}

void Shader::setSRV(ShaderType type, Slice<ID3D11ShaderResourceView *> textures) {
	if (type & ShaderType::Vertex)   gfx::context->VSSetShaderResources(0, (UINT)textures.len, textures.data);
	if (type & ShaderType::Fragment) gfx::context->PSSetShaderResources(0, (UINT)textures.len, textures.data);
	if (type & ShaderType::Compute)  gfx::context->CSSetShaderResources(0, (UINT)textures.len, textures.data);
}

void Shader::cleanup() {
	vert_sh.destroy();
	pixel_sh.destroy();
	compute_sh.destroy();
	layout.destroy();
	for (Buffer &buf : buffers) buf.cleanup();
	buffers.clear();
}

void Shader::bind() {
	gfx::context->IASetInputLayout(layout);
	gfx::context->VSSetShader(vert_sh, nullptr, 0);
	gfx::context->PSSetShader(pixel_sh, nullptr, 0);
	gfx::context->CSSetShader(compute_sh, nullptr, 0);
	gfx::context->PSSetSamplers(0, 1, &sampler);
}

void Shader::unbind() {
	// todo, maybe remove?
}

void Shader::dispatch(const vec3u &threads, Slice<ID3D11ShaderResourceView *> srvs, Slice<ID3D11UnorderedAccessView *> uavs) {
	if (!compute_sh) return;

	gfx::context->CSSetShader(compute_sh, nullptr, 0);
		if (!srvs.empty()) gfx::context->CSSetShaderResources(0, (UINT)srvs.len, srvs.data);
		if (!uavs.empty()) gfx::context->CSSetUnorderedAccessViews(0, (UINT)uavs.len, uavs.data, nullptr);

		gfx::context->Dispatch(threads.x, threads.y, threads.z);
		
		// set the resources back to NULL
		// actually horrible, terrible, probably very inefficent; but realistically i'm only
		// going to use 1/2 resources.
		ID3D11ShaderResourceView *null_srv[] = { nullptr };
		ID3D11UnorderedAccessView *null_uav[] = { nullptr };
		for (size_t i = 0; i < srvs.len; ++i) {
			gfx::context->CSSetShaderResources((UINT)i, 1, null_srv);
		}
		for (size_t i = 0; i < uavs.len; ++i) {
			gfx::context->CSSetUnorderedAccessViews((UINT)i, 1, null_uav, nullptr);
		}

	gfx::context->CSSetShader(nullptr, nullptr, 0);
}

DynamicShader::DynamicShader(DynamicShader &&s) {
	*this = std::move(s);
}

DynamicShader::~DynamicShader() {
	cleanup();
}

DynamicShader &DynamicShader::operator=(DynamicShader &&s) {
	if (this != &s) {
		std::swap(shader, s.shader);
		std::swap(watcher, s.watcher);
	}
	return *this;
}

bool DynamicShader::init(const char *vertex, const char *fragment, const char *compute) {
	if (!vertex && !fragment && !compute) return false;

	bool compiled = true;

	// add files to watchlist
	if (vertex)   compiled &= addFileWatch(vertex,   ShaderType::Vertex);
	if (fragment) compiled &= addFileWatch(fragment, ShaderType::Fragment);
	if (compute)  compiled &= addFileWatch(compute,  ShaderType::Compute);

	return compiled;
}

bool DynamicShader::addFileWatch(const char *name, ShaderType type) {
	const char *filename = str::format("%s.hlsl", name);
	watcher.watchFile(filename, (void *)type);
#ifdef NDEBUG
	if (file::exists(str::format("shaders/bin/%s.cso", name))) {
		bool success = false;
		switch (type) {
		case ShaderType::Vertex:   success = shader.loadVertex(name); break;
		case ShaderType::Fragment: success = shader.loadFragment(name); break;
		case ShaderType::Compute:  success = shader.loadCompute(name); break;
		}
		if (!success) {
			err("couldn't load shader %s from file", name);
			return false;
		}
	}
	else if (dxptr<ID3DBlob> blob = compileShader(filename, type)) {
		bool success = false;
		switch (type) {
		case ShaderType::Vertex:   success = shader.loadVertex(blob->GetBufferPointer(), blob->GetBufferSize()); break;
		case ShaderType::Fragment: success = shader.loadFragment(blob->GetBufferPointer(), blob->GetBufferSize()); break;
		case ShaderType::Compute:  success = shader.loadCompute(blob->GetBufferPointer(), blob->GetBufferSize()); break;
		}
		if (!success) {
			err("couldn't load shader %s from file", name);
			return false;
		}
	}
	else {
		return true;
	}
#else
	if (dxptr<ID3DBlob> blob = compileShader(filename, type)) {
		bool success = false;
		switch (type) {
		case ShaderType::Vertex:   success = shader.loadVertex(blob->GetBufferPointer(), blob->GetBufferSize()); break;
		case ShaderType::Fragment: success = shader.loadFragment(blob->GetBufferPointer(), blob->GetBufferSize()); break;
		case ShaderType::Compute:  success = shader.loadCompute(blob->GetBufferPointer(), blob->GetBufferSize()); break;
		}
		if (!success) {
			err("couldn't load shader %s from file", name);
			return false;
		}
	}
	else {
		return true;
	}
#endif

	return true;
}

void DynamicShader::cleanup() {
	shader.cleanup();
}

void DynamicShader::poll() {
	updated = ShaderType::None;
	watcher.update();

	auto file = watcher.getChangedFiles();
	while (file) {
		ShaderType type = (ShaderType)((uintptr_t)file->custom_data);
		if (dxptr<ID3DBlob> blob = compileShader(file->name.get(), type)) {
			info("re-compiled shader %s successfully", file->name.get());
			switch (type) {
			case ShaderType::Vertex:   shader.loadVertex(blob->GetBufferPointer(), blob->GetBufferSize());   break;
			case ShaderType::Fragment: shader.loadFragment(blob->GetBufferPointer(), blob->GetBufferSize()); break;
			case ShaderType::Compute:  shader.loadCompute(blob->GetBufferPointer(), blob->GetBufferSize());  break;
			}
			updated |= type;
		}

		file = watcher.getChangedFiles(file);
	}
}

static dxptr<ID3DBlob> compileShader(const char *filename, ShaderType type) {
	const char *type_str = "";
	switch (type) {
	case ShaderType::Vertex:   type_str = "vs"; break;
	case ShaderType::Fragment: type_str = "ps"; break;
	case ShaderType::Compute:  type_str = "cs"; break;
	}

	char target[16];
	mem::zero(target);

	file::MemoryBuf filedata = file::read(str::format("shaders/%s", filename));

	dxptr<ID3DBlob> blob = nullptr;
	dxptr<ID3DBlob> err_blob = nullptr;

	UINT flags = 0;
#ifdef _DEBUG
	flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	HRESULT hr = D3DCompile(
		filedata.data.get(),
		filedata.size,
		nullptr, 
		nullptr, 
		nullptr, 
		"main", 
		str::format("%s_5_0", type_str), 
		flags,
		0, 
		&blob, 
		&err_blob
	);

	if (FAILED(hr)) {
		err("couldn't compile shader %s", filename);
		if (err_blob) {
			err((const char *)err_blob->GetBufferPointer());
		}
		blob.destroy();
	}

	return blob;
}