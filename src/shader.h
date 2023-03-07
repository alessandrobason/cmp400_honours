#pragma once

#include <vector>
#include <string>
#include <memory>
#include <stddef.h>

#include "d3d11_fwd.h"
#include "matrix.h"
#include "buffer.h"
#include "timer.h"
#include "slice.h"
#include "utils.h"

struct Shader {
	Shader() = default;
	Shader(const Shader &s) = delete;
	Shader(Shader &&s);
	~Shader();

	Shader &operator=(Shader &&s);

	bool loadVertex(const char *filename);
	bool loadVertex(const void *data, size_t len);
	bool loadFragment(const char *filename);
	bool loadFragment(const void *data, size_t len);
	bool loadCompute(const char *filename);
	bool loadCompute(const void *data, size_t len);
	bool load(const char *vertex, const char *fragment, const char *compute);

	int addBuffer(size_t type_size, Usage usage = Usage::Default, bool can_write = true, bool can_read = false);
	int addBuffer(size_t type_size, Usage usage, const void *initial_data, size_t data_count = 1, bool can_write = true, bool can_read = false);
	template<typename T>
	int addBuffer(Usage usage = Usage::Default, bool can_write = true, bool can_read = false) { return addBuffer(sizeof(T), usage, can_write, can_read); }
	template<typename T>
	int addBuffer(Usage usage, const void *initial_data, size_t data_count = 1, bool can_write = true, bool can_read = false) { 
		return addBuffer(sizeof(T), usage, initial_data, data_count, can_write, can_read); 
	}

	Buffer *getBuffer(int index);

	bool addSampler();
	void setSRV(ShaderType type, Slice<ID3D11ShaderResourceView*> textures);

	void cleanup();

	//bool update(float time);
	void bind();
	void unbind();

	void dispatch(const vec3u &threads, Slice<int> cbuffers = {}, Slice<ID3D11ShaderResourceView *> srvs = {}, Slice<ID3D11UnorderedAccessView *> uavs = {});

	dxptr<ID3D11VertexShader> vert_sh = nullptr;
	dxptr<ID3D11PixelShader> pixel_sh = nullptr;
	dxptr<ID3D11ComputeShader> compute_sh = nullptr;
	dxptr<ID3D11InputLayout> layout = nullptr;
	std::vector<Buffer> buffers;
	dxptr<ID3D11SamplerState> sampler = nullptr;
};

struct DynamicShader {
	DynamicShader() = default;
	DynamicShader(const DynamicShader &s) = delete;
	DynamicShader(DynamicShader &&s);
	~DynamicShader();

	DynamicShader &operator=(DynamicShader &&s);

	bool init(const char *vertex, const char *fragment, const char *compute);
	void cleanup();

	void poll();
	bool hasUpdated() const { return updated != ShaderType::None; }
	ShaderType getChanged() const { return updated; }

	Shader shader;

private:
	bool addFileWatch(const char *name, ShaderType type);

	file::Watcher watcher = "shaders/";
	ShaderType updated = ShaderType::None;
};

// == ENUM CLASS OPERATORS ===============================================

inline ShaderType &operator|=(ShaderType &self, ShaderType b) {
	self = ShaderType((uint8_t)self | (uint8_t)b);
	return self;
}

inline uint8_t operator&(ShaderType a, ShaderType b) {
	return (uint8_t)a & (uint8_t)b;
}