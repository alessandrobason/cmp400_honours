#include "shader.h"

#include <d3d11.h>

#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler")

#include "system.h"
#include "tracelog.h"
#include "mesh.h"
#include "buffer.h"
#include "gfx_factory.h"
#include "widgets.h"
#include "fs.h"

static GFXFactory<Shader> shader_factory;

static dxptr<ID3DBlob> compileShader(const char *filename, ShaderType type, Slice<ShaderMacro> macros = {});

struct ShaderHandler {
	void add(const char *filename, Handle<Shader> handle);
	void poll();
	fs::Watcher watcher = "shaders/";
	arr<Handle<Shader>> update_list;
} sh_handler;

void pollShaders() {
	sh_handler.poll();
}

Shader::Shader(Shader &&s) {
	*this = mem::move(s);
}

Shader::~Shader() {
	cleanup();
}

Shader &Shader::operator=(Shader &&s) {
	if (this != &s) {
		mem::swap(shader, s.shader);
		mem::swap(extra, s.extra);
		// mem::swap(buffers, s.buffers);
		mem::swap(shader_type, s.shader_type);
	}
	return *this;
}

Handle<Shader> Shader::make() {
	return shader_factory.getNew();
}

Handle<Shader> Shader::load(const char *filename, ShaderType type, bool hot_reload) {
	Shader *shader = shader_factory.getNew();
	bool success = false;

	if (fs::MemoryBuf buf = fs::read(str::format("shaders/bin/%s.cso", filename))) {
		switch (type) {
			case ShaderType::Vertex:	success = shader->loadVertex(buf.data.get(), buf.size);   break;
			case ShaderType::Fragment:	success = shader->loadFragment(buf.data.get(), buf.size); break;
			case ShaderType::Compute:	success = shader->loadCompute(buf.data.get(), buf.size);  break;
		}
	}

	if (!success) {
		err("couldn't load shader from file %s", filename);
		shader_factory.popLast();
		return nullptr;
	}

	if (hot_reload) sh_handler.add(filename, shader);
	return shader;
}

Handle<Shader> Shader::compile(const char *filename, ShaderType type, Slice<ShaderMacro> macros, bool hot_reload) {
	Shader *shader = shader_factory.getNew();
	bool success = false;

	if (dxptr<ID3DBlob> blob = compileShader(filename, type, macros)) {
		void *data = blob->GetBufferPointer();
		size_t len = blob->GetBufferSize();

		switch (type) {
			case ShaderType::Vertex:	success = shader->loadVertex(data, len);   break;
			case ShaderType::Fragment:	success = shader->loadFragment(data, len); break;
			case ShaderType::Compute:	success = shader->loadCompute(data, len);  break;
		}
	}

	if (!success) {
		err("couldn't compile shader from file %s", filename);
		shader_factory.popLast();
		return nullptr;
	}

	if (hot_reload) sh_handler.add(filename, shader);
	return shader;
}

bool Shader::hasUpdated(Handle<Shader> handle) {
	size_t index = sh_handler.update_list.find(handle);
	if (index == SIZE_MAX) return false;
	sh_handler.update_list.remove(index);
	return true;
}

bool Shader::loadVertex(const void *data, size_t len) {
	shader.destroy();
	shader_type = ShaderType::Vertex;
	HRESULT hr = gfx::device->CreateVertexShader(data, len, nullptr, (ID3D11VertexShader **)&shader);
	if (FAILED(hr)) {
		err("couldn't create vertex shader");
		return false;
	}

	// no need to recreate the layout if its already loaded
	if (extra) {
		return true;
	}

	D3D11_INPUT_ELEMENT_DESC layout_desc[] = {
		"POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0, (UINT)offsetof(Mesh::Vertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0,
		"TEXCOORDS", 0, DXGI_FORMAT_R32G32_FLOAT,       0, (UINT)offsetof(Mesh::Vertex, uv),       D3D11_INPUT_PER_VERTEX_DATA, 0,
	};

	hr = gfx::device->CreateInputLayout(
		layout_desc, ARRLEN(layout_desc),
		data, len,
		(ID3D11InputLayout **)&extra
	);
	if (FAILED(hr)) {
		err("couldn't create input layout");
		return false;
	}

	return true;
}

bool Shader::loadFragment(const void *data, size_t len) {
	shader.destroy();
	shader_type = ShaderType::Fragment;
	HRESULT hr = gfx::device->CreatePixelShader(data, len, nullptr, (ID3D11PixelShader **)&shader);
	if (FAILED(hr)) {
		err("couldn't create fragment shader");
		return false;
	}

	return addSampler();
}

bool Shader::loadCompute(const void *data, size_t len) {
	shader.destroy();
	shader_type = ShaderType::Compute;
	HRESULT hr = gfx::device->CreateComputeShader(data, len, nullptr, (ID3D11ComputeShader**)&shader);
	if (FAILED(hr)) {
		err("couldn't create compute shader");
		return false;
	}

	return true;
}

bool Shader::addSampler() {
	extra.destroy();

	D3D11_SAMPLER_DESC desc;
	mem::zero(desc);
	desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	desc.ComparisonFunc = D3D11_COMPARISON_NEVER;

	HRESULT hr = gfx::device->CreateSamplerState(&desc, (ID3D11SamplerState **)&extra);
	if (FAILED(hr)) {
		err("couldn't create sampler state");
		return false;
	}

	return true;
}

void Shader::bindSRV(Slice<ID3D11ShaderResourceView *> textures) {
	switch (shader_type) {
		case ShaderType::Vertex:   gfx::context->VSSetShaderResources(0, (UINT)textures.len, textures.data); break;
		case ShaderType::Fragment: gfx::context->PSSetShaderResources(0, (UINT)textures.len, textures.data); break;
		case ShaderType::Compute:  gfx::context->CSSetShaderResources(0, (UINT)textures.len, textures.data); break;
	}
}

void Shader::unbindSRV(unsigned int count) {
	static ID3D11ShaderResourceView *null_srv[10] = {};
	assert(count < ARRLEN(null_srv));

	switch (shader_type) {
		case ShaderType::Vertex:   gfx::context->VSSetShaderResources(0, count, null_srv); break;
		case ShaderType::Fragment: gfx::context->PSSetShaderResources(0, count, null_srv); break;
		case ShaderType::Compute:  gfx::context->CSSetShaderResources(0, count, null_srv); break;
	}
}

void Shader::cleanup() {
	shader.destroy();
	extra.destroy();
}

void Shader::bind() {
	switch (shader_type) {
	case ShaderType::Vertex:
		gfx::context->IASetInputLayout((ID3D11InputLayout *)extra.get());
		gfx::context->VSSetShader((ID3D11VertexShader *)shader.get(), nullptr, 0);
		break;
	case ShaderType::Fragment:
		gfx::context->PSSetSamplers(0, 1, (ID3D11SamplerState**)&extra);
		gfx::context->PSSetShader((ID3D11PixelShader *)shader.get(), nullptr, 0);
		break;
	case ShaderType::Compute:
		if (extra) gfx::context->CSSetSamplers(0, 1, (ID3D11SamplerState **)&extra);
		gfx::context->CSSetShader((ID3D11ComputeShader*)shader.get(), nullptr, 0);
		break;
	}
}

void Shader::bind(Slice<Handle<Buffer>> cbuffers, Slice<ID3D11ShaderResourceView *> srvs) {
	bind();
	bindCBuffers(cbuffers);
	bindSRV(srvs);
}

void Shader::bindCBuf(Handle<Buffer> handle, unsigned int slot) {
	if (Buffer *buf = handle.get()) {
		buf->bindCBuffer(shader_type, slot);
	}
}

void Shader::bindCBuffers(Slice<Handle<Buffer>> handles) {
	int slot = 0;
	for (Handle<Buffer> handle : handles) {
		if (Buffer *buf = handle.get()) {
			buf->bindCBuffer(shader_type, slot++);
		}
	}
}

void Shader::unbind(int cbuf_count, int srv_count) {
	unbindCBuffers(cbuf_count);
	unbindSRV(srv_count);
}

void Shader::unbindCBuf(unsigned int slot) {
	Buffer::unbindCBuffer(shader_type, slot);
}

void Shader::unbindCBuffers(int count, unsigned int slot) {
	Buffer::unbindCBuffer(shader_type, slot, count);
}

void Shader::dispatch(
	const vec3u &threads, 
	Slice<Handle<Buffer>> cbuffers, 
	Slice<ID3D11ShaderResourceView *> srvs, 
	Slice<ID3D11UnorderedAccessView *> uavs
) {
	if (!shader) return;

	if (extra) gfx::context->CSSetSamplers(0, 1, (ID3D11SamplerState **)&extra);

	gfx::context->CSSetShader((ID3D11ComputeShader *)shader.get(), nullptr, 0);

		UINT cbuffers_count = 0;

		if (!cbuffers.empty()) {
			for (Handle<Buffer> handle : cbuffers) {
				if (Buffer *buf = handle.get()) {
					buf->bindCBuffer(ShaderType::Compute, cbuffers_count++);
				}
			}
		}

		if (!srvs.empty()) gfx::context->CSSetShaderResources(0, (UINT)srvs.len, srvs.data);
		if (!uavs.empty()) gfx::context->CSSetUnorderedAccessViews(0, (UINT)uavs.len, uavs.data, nullptr);

		gfx::context->Dispatch(threads.x, threads.y, threads.z);
		
		Buffer::unbindCBuffer(ShaderType::Compute, 0, cbuffers_count);
		Buffer::unbindSRV(ShaderType::Compute, 0, srvs.len);
		Buffer::unbindUAV(0, uavs.len);

	ID3D11SamplerState *null_sampler[] = { nullptr };
	gfx::context->CSSetSamplers(0, 1, null_sampler);
	gfx::context->CSSetShader(nullptr, nullptr, 0);
}

/* ==========================================
   ============= SHADER HANDLER =============
   ========================================== */

void ShaderHandler::add(const char *filename, Handle<Shader> handle) {
	watcher.watchFile(filename, handle.get());
}

void ShaderHandler::poll() {
	watcher.update();

	auto file = watcher.getChangedFiles();
	while (file) {
		Shader *shader = (Shader *)file->custom_data;

		if (dxptr<ID3DBlob> blob = compileShader(file->name.get(), shader->shader_type)) {
			info("re-compiled shader %s successfully", file->name.get());
			widgets::addMessage(LogLevel::Info, str::format("re-compiled shader %s successfully", file->name.get()));

			void *data = blob->GetBufferPointer();
			size_t len = blob->GetBufferSize();

			switch (shader->shader_type) {
				case ShaderType::Vertex:   shader->loadVertex(data, len);   break;
				case ShaderType::Fragment: shader->loadFragment(data, len); break;
				case ShaderType::Compute:  shader->loadCompute(data, len);  break;
			}

			if (!update_list.contains(shader)) {
				update_list.push(shader);
			}
		}

		file = watcher.getChangedFiles(file);
	}
}

static dxptr<ID3DBlob> compileShader(const char *filename, ShaderType type, Slice<ShaderMacro> macros) {
	// check that macros is null-terminated
	if (macros) {
		const ShaderMacro &null_macro = macros.back();
		if (null_macro.name != nullptr || null_macro.value != nullptr) {
			err("macros should be null terminated");
			return nullptr;
		}
	}
	
	const char *type_str = "";
	switch (type) {
		case ShaderType::Vertex:   type_str = "vs"; break;
		case ShaderType::Fragment: type_str = "ps"; break;
		case ShaderType::Compute:  type_str = "cs"; break;
	}

	fs::MemoryBuf filedata = fs::read(str::format("shaders/%s", filename));

	if (!filedata) {
		err("couldn't open shader file %s to compile", filename);
		return nullptr;
	}

	info("compiling %s, type: %s", filename, type_str);

	dxptr<ID3DBlob> blob = nullptr;
	dxptr<ID3DBlob> err_blob = nullptr;

	UINT flags = 0;
#ifdef _DEBUG
	flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	HRESULT hr = D3DCompile(
		filedata.data.get(),
		filedata.size,
		filename,
		(D3D_SHADER_MACRO *)macros.data, 
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"main",
		str::format("%s_5_0", type_str), 
		flags,
		0,
		&blob,
		&err_blob
	);

	if (FAILED(hr)) {
		err("couldn't compile shader %s", filename);
		widgets::addMessage(LogLevel::Error, str::format("Couldn't compile shader %s", filename));
		if (err_blob) {
			err((const char *)err_blob->GetBufferPointer());
		}
		blob.destroy();
	}

	return blob;
}