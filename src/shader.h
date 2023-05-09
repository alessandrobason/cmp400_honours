#pragma once
 
#include "gfx_common.h"
#include "handle.h"
#include "slice.h"

struct Buffer;
template<typename T> struct vec3T;
using vec3u = vec3T<unsigned int>;

struct ShaderMacro {
	const char *name = nullptr;
	const char *value = nullptr;
};

struct Shader {
	Shader() = delete;
	Shader(const Shader &s) = delete;
	Shader(Shader &&s);
	~Shader();
	Shader &operator=(Shader &&s);

	// -- handle stuff --
	static Handle<Shader> make();
	static Handle<Shader> load(const char *filename, ShaderType type, bool hot_reload = true);
	static Handle<Shader> compile(const char *filename, ShaderType type, Slice<ShaderMacro> macros = {}, bool hot_reload = true);
	static bool hasUpdated(Handle<Shader> handle);
	// ------------------

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
