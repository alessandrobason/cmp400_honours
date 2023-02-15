#include "shader.h"

#include <d3d11.h>

#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler")

#include "system.h"
#include "tracelog.h"
#include "utils.h"
#include "macros.h"
#include "mesh.h"

static ID3DBlob *compileShader(const char *filename, ShaderType type);

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
	SAFE_RELEASE(vert_sh);

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
	SAFE_RELEASE(pixel_sh);

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
	SAFE_RELEASE(compute_sh);

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

void Shader::setSRV(Slice<ID3D11ShaderResourceView *> textures) {
	gfx::context->PSSetShaderResources(0, (UINT)textures.len, textures.data);
}

void Shader::cleanup() {
	SAFE_RELEASE(vert_sh);
	SAFE_RELEASE(pixel_sh);
	SAFE_RELEASE(compute_sh);
	SAFE_RELEASE(layout);
	for (Buffer &buf : buffers) buf.cleanup();
	buffers.clear();
}

#if 0
bool Shader::update(float time) {
	if (pixel_sh) {
		ShaderDataBuffer *buf = shader_data_buf.map<ShaderDataBuffer>();
		if (!buf) {
			err("could not map subresource shader data buffer");
			return false;
		}

		buf->time = time;

		shader_data_buf.unmapPS();
	}

	if (compute_sh) {

	}

	return true;
}
#endif

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

DynamicShader::DynamicShader(DynamicShader &&s) {
	*this = std::move(s);
}

DynamicShader::~DynamicShader() {
	cleanup();
}

DynamicShader &DynamicShader::operator=(DynamicShader &&s) {
	if (this != &s) {
		std::swap(shader, s.shader);
		std::swap(watched_files, s.watched_files);
		std::swap(changed_files, s.changed_files);
		std::swap(handle, s.handle);
	}
	return *this;
}

bool DynamicShader::init(const char *vertex, const char *fragment, const char *compute) {
	if (!vertex && !fragment && !compute) return false;

	HANDLE dir_handle = CreateFile(
		TEXT("shaders/"),
		FILE_LIST_DIRECTORY,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
		NULL
	);

	if (dir_handle == INVALID_HANDLE_VALUE) {
		err("couldn't open shader directory handle");
		return false;
	}

	overlapped.hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

	BOOL success = ReadDirectoryChangesW(
		dir_handle,
		change_buf, sizeof(change_buf),
		FALSE, // watch subtree
		FILE_NOTIFY_CHANGE_LAST_WRITE,
		nullptr, // bytes returned
		(OVERLAPPED *)&overlapped,
		nullptr // completion routine
	);

	if (!success) {
		err("couldn't start watching shader directory");
		return false;
	}

	handle = dir_handle;
	info("handle: %p", handle);

	bool compiled = true;

	// add files to watchlist
	if (vertex)   compiled &= addFileWatch(vertex,   ShaderType::Vertex);
	if (fragment) compiled &= addFileWatch(fragment, ShaderType::Fragment);
	if (compute)  compiled &= addFileWatch(compute,  ShaderType::Compute);

	return compiled;
}

bool DynamicShader::addFileWatch(const char *name, ShaderType type) {
	const char *filename = str::format("%s.hlsl", name);
	watched_files.emplace_back(filename, type);
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
		return false;
	}

	return true;
}

void DynamicShader::cleanup() {
	shader.cleanup();
	if (handle) {
		CloseHandle((HANDLE)handle);
	}
}

void DynamicShader::poll() {
	if (!handle || handle == INVALID_HANDLE_VALUE) {
		return;
	}

	is_dirty = false;

	tryUpdate();

	DWORD wait_status = WaitForMultipleObjects(1, &overlapped.hEvent, FALSE, 0);

	if (wait_status == WAIT_TIMEOUT) {
		return;
	}
	else if (wait_status != WAIT_OBJECT_0) {
		warn("unhandles wait_status value: %u", wait_status);
		return;
	}

	DWORD bytes_transferred;
	GetOverlappedResult((HANDLE)handle, (OVERLAPPED *)&overlapped, &bytes_transferred, FALSE);

	FILE_NOTIFY_INFORMATION *event = (FILE_NOTIFY_INFORMATION *)change_buf;

	while (true) {
		switch (event->Action) {
			case FILE_ACTION_MODIFIED:
			{
				char namebuf[1024];
				if (!str::wideToAnsi(event->FileName, event->FileNameLength / sizeof(WCHAR), namebuf, sizeof(namebuf))) {
					err("name too long: %S -- %u", event->FileName, event->FileNameLength);
					break;
				}

				for (size_t i = 0; i < watched_files.size(); ++i) {
					if (watched_files[i].name == namebuf) {
						addChanged(i);
						break;
					}
				}

				break;
			}
		}

		if (event->NextEntryOffset) {
			*((uint8_t **)&event) += event->NextEntryOffset;
		}
		else {
			break;
		}
	}

	BOOL success = ReadDirectoryChangesW(
		(HANDLE)handle, 
		change_buf, sizeof(change_buf), 
		FALSE,
		FILE_NOTIFY_CHANGE_LAST_WRITE,
		nullptr, 
		(OVERLAPPED *)&overlapped,
		nullptr
	);
	if (!success) {
		err("call to ReadDirectoryChangesW failed");
		return;
	}
}

void DynamicShader::addChanged(size_t index) {
	for (const auto &file : changed_files) {
		if (file.index == index) {
			// no need to re-add it to the list
			return;
		}
	}

	changed_files.emplace_back(index);
}

void DynamicShader::tryUpdate() {
	for (auto file = changed_files.begin(); file != changed_files.end(); ) {
		if (file->clock.after(0.1f)) {
			WatchedFile &watch = watched_files[file->index];
			if (dxptr<ID3DBlob> blob = compileShader(watch.name.c_str(), watch.type)) {
				info("re-compiled shader %s successfully", watch.name.c_str());
				switch (watch.type) {
				case ShaderType::Vertex:   shader.loadVertex(blob->GetBufferPointer(), blob->GetBufferSize());   break;
				case ShaderType::Fragment: shader.loadFragment(blob->GetBufferPointer(), blob->GetBufferSize()); break;
				case ShaderType::Compute:  shader.loadCompute(blob->GetBufferPointer(), blob->GetBufferSize());  break;
				}
				is_dirty = true;
			}
			file = changed_files.erase(file);
		}
		else {
			++file;
		}
	}
}

static ID3DBlob *compileShader(const char *filename, ShaderType type) {
	const char *type_str = "";
	switch (type) {
	case ShaderType::Vertex:   type_str = "vs"; break;
	case ShaderType::Fragment: type_str = "ps"; break;
	case ShaderType::Compute:  type_str = "cs"; break;
	}

	char target[16];
	mem::zero(target);

	file::MemoryBuf filedata = file::read(str::format("shaders/%s", filename));

	ID3DBlob *blob = nullptr;
	dxptr<ID3DBlob> err_blob = nullptr;

	HRESULT hr = D3DCompile(
		filedata.data.get(),
		filedata.size,
		nullptr, 
		nullptr, 
		nullptr, 
		"main", 
		str::format("%s_5_0", type_str), 
		0, 
		0, 
		&blob, 
		(ID3DBlob **)&err_blob
	);

	if (FAILED(hr)) {
		err("couldn't compile shader %s", filename);
		if (err_blob) {
			err((const char *)err_blob->GetBufferPointer());
		}
		SAFE_RELEASE(blob);
		return nullptr;
	}

	return blob;
}