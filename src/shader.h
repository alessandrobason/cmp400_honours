#pragma once
 
#include "gfx_common.h"
#include "handle.h"
#include "utils.h"
//#include "vec.h"

struct Buffer;
template<typename T> struct vec3T;
using vec3u = vec3T<unsigned int>;

struct Shader {
	Shader() = default;
	Shader(const Shader &s) = delete;
	Shader(Shader &&s);
	~Shader();

	Shader &operator=(Shader &&s);

	bool load(const char *filename, ShaderType type);

	bool loadVertex(const void *data, size_t len);
	bool loadFragment(const void *data, size_t len);
	bool loadCompute(const void *data, size_t len);

	bool addSampler();
	void bindSRV(Slice<ID3D11ShaderResourceView *> textures);
	void unbindSRV(unsigned int count = 1);

	void cleanup();

	void bind();
	void bind(Slice<Handle<Buffer>> cbuffers, Slice<ID3D11ShaderResourceView *> srvs);
	void bindCBuf(Handle<Buffer> handle, unsigned int slot = 0);
	void bindCBuffers(Slice<Handle<Buffer>> handles = {});
	void unbind(int cbuf_count = 0, int srv_count = 0);
	void unbindCBuf(unsigned int slot = 0);
	void unbindCBuffers(int count = 1, unsigned int slot = 0);

	void dispatch(const vec3u &threads, Slice<Handle<Buffer>> cbuffers = {}, Slice<ID3D11ShaderResourceView *> srvs = {}, Slice<ID3D11UnorderedAccessView *> uavs = {});

	dxptr<ID3D11DeviceChild> shader = nullptr;
	// either input layout (if VS) or sampler state (if PS)
	dxptr<ID3D11DeviceChild> extra = nullptr;
	ShaderType shader_type = ShaderType::None;
};

struct DynamicShader {
	DynamicShader() = default;
	DynamicShader(const DynamicShader &s) = delete;
	DynamicShader(DynamicShader &&s);
	~DynamicShader();

	DynamicShader &operator=(DynamicShader &&s);

	int add(const char *name, ShaderType type);
	void cleanup();

	void poll();
	bool hasUpdated() const { return has_updated; }
	bool hasUpdated(int index) const;

	Shader *get(int index);

private:
	bool addFileWatch(const char *name, ShaderType type);

	arr<Shader> shaders;
	arr<bool> update_list;
	file::Watcher watcher = "shaders/";
	bool has_updated = false;
};
