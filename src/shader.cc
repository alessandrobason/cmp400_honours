#include "gfx.h"

#include <d3d11.h>

#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler")

#include "system.h"
#include "tracelog.h"
#include "utils.h"
#include "macros.h"
#include "gfx.h"

static dxptr<ID3DBlob> compileShader(const char *filename, ShaderType type);

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
		mem::swap(buffers, s.buffers);
		mem::swap(shader_type, s.shader_type);
	}
	return *this;
}

bool Shader::load(const char* filename, ShaderType type) {
	assert(shader_type == ShaderType::None);
	shader_type = type;
	shader.destroy();

	file::MemoryBuf buf = file::read(str::format("shaders/bin/%s.cso", filename));

	if (!buf.data) {
		err("couldn't read shader file %s", filename);
		return false;
	}

	switch (type) {
	case ShaderType::Vertex:	return loadVertex(buf.data.get(), buf.size);
	case ShaderType::Fragment:	return loadFragment(buf.data.get(), buf.size);
	case ShaderType::Compute:	return loadCompute(buf.data.get(), buf.size);
	}

	return false;
}

bool Shader::loadVertex(const void *data, size_t len) {
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
	shader_type = ShaderType::Fragment;
	HRESULT hr = gfx::device->CreatePixelShader(data, len, nullptr, (ID3D11PixelShader **)&shader);
	if (FAILED(hr)) {
		err("couldn't create fragment shader");
		return false;
	}

	return true;
}

bool Shader::loadCompute(const void *data, size_t len) {
	shader_type = ShaderType::Compute;
	HRESULT hr = gfx::device->CreateComputeShader(data, len, nullptr, (ID3D11ComputeShader**)&shader);
	if (FAILED(hr)) {
		err("couldn't create compute shader");
		return false;
	}

	return true;
}

int Shader::addBuffer(size_t type_size, Buffer::Usage usage, bool can_write, bool can_read) {
	Buffer newbuf;
	if (!newbuf.create(type_size, usage, can_write, can_read)) {
		err("couldn't create new buffer!");
		return -1;
	}
	int index = (int)buffers.size();
	buffers.emplace_back(mem::move(newbuf));
	return index;
}

int Shader::addBuffer(size_t type_size, Buffer::Usage usage, const void *initial_data, size_t data_count, bool can_write, bool can_read) {
	Buffer newbuf;
	if (!newbuf.create(type_size, usage, initial_data, data_count, can_write, can_read)) {
		err("couldn't create new buffer!");
		return -1;
	}
	int index = (int)buffers.size();
	buffers.emplace_back(mem::move(newbuf));
	return index;
}

int Shader::addStructuredBuf(size_t type_size, size_t count, const void* initial_data) {
	Buffer newbuf;
	if (!newbuf.createStructured(type_size, count, initial_data)) {
		err("couldn't create new structured buffer!");
		return -1;
	}
	int index = (int)buffers.size();
	buffers.emplace_back(mem::move(newbuf));
	return index;
}

Buffer *Shader::getBuffer(int index) {
	if (index < 0 || index >= (int)buffers.size()) {
		return nullptr;
	}
	return &buffers[index];
}

bool Shader::addSampler() {
	assert(shader_type == ShaderType::Fragment);
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

void Shader::setSRV(Slice<ID3D11ShaderResourceView *> textures) {
	switch (shader_type) {
		case ShaderType::Vertex:   gfx::context->VSSetShaderResources(0, (UINT)textures.len, textures.data); break;
		case ShaderType::Fragment: gfx::context->PSSetShaderResources(0, (UINT)textures.len, textures.data); break;
		case ShaderType::Compute:  gfx::context->CSSetShaderResources(0, (UINT)textures.len, textures.data); break;
	}
}

void Shader::cleanup() {
	shader.destroy();
	extra.destroy();
	for (Buffer &buf : buffers) buf.cleanup();
	buffers.clear();
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
		gfx::context->CSSetShader((ID3D11ComputeShader*)shader.get(), nullptr, 0);
		break;
	}
}

void Shader::bindCBuf(int buffer, unsigned int slot) {
	if (Buffer *buf = getBuffer(buffer)) {
		buf->bindCBuffer(shader_type, slot);
	}
}

void Shader::bindCBuffers(Slice<int> cbuffers) {
	int slot = 0;
	for (int ind : cbuffers) {
		if (Buffer *buf = getBuffer(ind)) {
			buf->bindCBuffer(shader_type, slot++);
		}
	}
}

void Shader::unbindCBuf(unsigned int slot) {
	Buffer::unbindCBuffer(shader_type, slot);
}

void Shader::unbindCBuffers(int count, unsigned int slot) {
	if (count >= (int)buffers.size()) count = (int)buffers.size() - 1;
	Buffer::unbindCBuffer(shader_type, slot, count);
}

void Shader::dispatch(const vec3u &threads, Slice<int> cbuffers, Slice<ID3D11ShaderResourceView *> srvs, Slice<ID3D11UnorderedAccessView *> uavs) {
	if (!shader) return;

	gfx::context->CSSetShader((ID3D11ComputeShader *)shader.get(), nullptr, 0);

		UINT cbuffers_count = 0;

		if (!cbuffers.empty()) {
			for (int ind : cbuffers) {
				if (Buffer *buf = getBuffer(ind)) {
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

	gfx::context->CSSetShader(nullptr, nullptr, 0);
}

/* ==========================================
   ============= DYNAMIC SHADER =============
   ========================================== */

DynamicShader::DynamicShader(DynamicShader &&s) {
	*this = mem::move(s);
}

DynamicShader::~DynamicShader() {
	cleanup();
}

DynamicShader &DynamicShader::operator=(DynamicShader &&s) {
	if (this != &s) {
		mem::swap(shaders, s.shaders);
		mem::swap(watcher, s.watcher);
	}
	return *this;
}

int DynamicShader::add(const char* name, ShaderType type) {
	if (!name || type == ShaderType::None) return false;
	
	int index = (int)shaders.size();
	bool success = addFileWatch(name, type);
	return success ? index : -1;
}

Shader* DynamicShader::get(int index) {
	if (index < 0 || index >= shaders.size()) return nullptr;
	return &shaders[index];
}

bool DynamicShader::addFileWatch(const char *name, ShaderType type) {
	const char *filename = str::format("%s.hlsl", name);
	watcher.watchFile(filename, (void *)shaders.size());
	update_list.emplace_back(false);

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
		Shader new_shader;
		void *data = blob->GetBufferPointer();
		size_t len = blob->GetBufferSize();

		switch (type) {
			case ShaderType::Vertex:	success = new_shader.loadVertex(data, len);   break;
			case ShaderType::Fragment:	success = new_shader.loadFragment(data, len); break;
			case ShaderType::Compute:	success = new_shader.loadCompute(data, len);  break;
		}

		if (!success) {
			err("couldn't load shader %s from file", name);
			return false;
		}

		shaders.emplace_back(mem::move(new_shader));
	}
	else {
		return false;
	}
#endif

	return true;
}

void DynamicShader::cleanup() {
	for (Shader &sh : shaders) sh.cleanup();
	shaders.clear();
}

extern void addMessageToWidget(LogLevel severity, const char* message);

void DynamicShader::poll() {
	update_list.fill(false);
	has_updated = false;
	watcher.update();

	auto file = watcher.getChangedFiles();
	while (file) {
		size_t index = (size_t)((uintptr_t)file->custom_data);
		assert(index < shaders.size());
		Shader &shader = shaders[index];

		if (dxptr<ID3DBlob> blob = compileShader(file->name.get(), shader.shader_type)) {
			info("re-compiled shader %s successfully", file->name.get());
			addMessageToWidget(LogLevel::Info, str::format("re-compiled shader %s successfully", file->name.get()));

			void *data = blob->GetBufferPointer();
			size_t len = blob->GetBufferSize();

			switch (shader.shader_type) {
				case ShaderType::Vertex:   shader.loadVertex(data, len);   break;
				case ShaderType::Fragment: shader.loadFragment(data, len); break;
				case ShaderType::Compute:  shader.loadCompute(data, len);  break;
			}

			has_updated = true;
			update_list[index] = true;
		}

		file = watcher.getChangedFiles(file);
	}
}

bool DynamicShader::hasUpdated(int index) const {
	if (index < 0 || index >= update_list.size()) return false;
	return update_list[index];
}

static dxptr<ID3DBlob> compileShader(const char *filename, ShaderType type) {
	const char *type_str = "";
	switch (type) {
		case ShaderType::Vertex:   type_str = "vs"; break;
		case ShaderType::Fragment: type_str = "ps"; break;
		case ShaderType::Compute:  type_str = "cs"; break;
	}

	file::MemoryBuf filedata = file::read(str::format("shaders/%s", filename));

	if (filedata.size == 0) {
		// we might be trying to read a file that is locked by some other application
		// for now, just ignore this, the file watcher already calls this another time anyway
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
		addMessageToWidget(LogLevel::Error, str::format("Couldn't compile shader %s", filename));
		if (err_blob) {
			err((const char *)err_blob->GetBufferPointer());
		}
		blob.destroy();
	}

	return blob;
}