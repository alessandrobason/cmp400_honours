#pragma once

#include "gfx_common.h"
#include "vec.h"
#include "colour.h"
#include "handle.h"

namespace thr { template<typename T> struct Promise; }

struct Texture2D {
	Texture2D() = delete;
	Texture2D(const Texture2D &rt) = delete;
	Texture2D(Texture2D &&rt);
	~Texture2D();
	Texture2D &operator=(Texture2D &&rt);

	// -- handle stuff --
	static Handle<Texture2D> make();
	static Handle<Texture2D> create(const vec2i &size, bool can_gpu_read = false);
	static Handle<Texture2D> load(const char *filename, bool can_gpu_read = false);
	static Handle<Texture2D> loadHDR(const char *filename, bool can_gpu_read = false);
	// ------------------

	bool init(const vec2i &size, bool can_gpu_read = false);
	bool loadFromFile(const char *filename, bool can_gpu_read = false);
	bool loadFromHDRFile(const char *filename, bool can_gpu_read = false);
	void cleanup();

	bool takeScreenshot(const char *base_name = "screenshot");
	void clear(Colour colour);
	void copyInto(Handle<Texture2D> handle);

	vec2i size = 0;
	dxptr<ID3D11Texture2D> texture = nullptr;
	dxptr<ID3D11ShaderResourceView> srv = nullptr;
	dxptr<ID3D11UnorderedAccessView> uav = nullptr;
};

struct Texture3D {
	enum class Type : uint8_t {
		uint8,
		uint16,
		uint32,
		sint8,
		sint16,
		sint32,
		float16,
		float32,
		r16g16_uint,
		r11g11b10_float,
		count,
	};

	Texture3D() = delete;
	Texture3D(const Texture3D &rt) = delete;
	Texture3D(Texture3D &&rt);
	~Texture3D();
	Texture3D &operator=(Texture3D &&rt);

	// -- handle stuff --
	static Handle<Texture3D> make();
	static Handle<Texture3D> create(const vec3u &texsize, Type type, const void *initial_data = nullptr);
	static Handle<Texture3D> create(int width, int height, int depth, Type type, const void *initial_data = nullptr);
	static Handle<Texture3D> load(const char *filename);
	// ------------------

	bool init(const vec3u &texsize, Type type, const void *initial_data = nullptr);
	bool init(int width, int height, int depth, Type type, const void *initial_data = nullptr);
	bool loadFromFile(const char *filename);
	bool save(const char *filename, bool overwrite = false, thr::Promise<bool> *promise = nullptr);
	void cleanup();
	Type getType();

	vec3i size = 0;
	dxptr<ID3D11Texture3D> texture = nullptr;
	dxptr<ID3D11UnorderedAccessView> uav = nullptr;
	dxptr<ID3D11ShaderResourceView> srv = nullptr;
};

struct RenderTexture : Texture2D {
	RenderTexture() = delete;
	RenderTexture(const RenderTexture &rt) = delete;
	RenderTexture(RenderTexture &&rt);
	~RenderTexture();
	RenderTexture &operator=(RenderTexture &&rt);

	// -- handle stuff --
	static Handle<RenderTexture> make();
	static Handle<RenderTexture> create(int width, int height);
	static Handle<RenderTexture> fromBackbuffer();
	// ------------------

	bool resize(int new_width, int new_height);
	bool reloadBackbuffer();
	void cleanup();

	void bind(ID3D11DepthStencilView *dsv = nullptr);
	void clear(Colour colour, ID3D11DepthStencilView *dsv = nullptr);

	dxptr<ID3D11RenderTargetView> rtv = nullptr;
};