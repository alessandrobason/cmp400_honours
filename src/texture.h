#pragma once

#include "gfx_common.h"
#include "vec.h"
#include "colour.h"

struct Texture2D {
	Texture2D() = default;
	Texture2D(const Texture2D &rt) = delete;
	Texture2D(Texture2D &&rt);
	~Texture2D();
	Texture2D &operator=(Texture2D &&rt);

	bool load(const char *filename);
	bool loadHDR(const char *filename);
	void cleanup();

	vec2i size = 0;
	dxptr<ID3D11Texture2D> texture = nullptr;
	dxptr<ID3D11ShaderResourceView> srv = nullptr;
};

struct Texture3D {
	enum class Type {
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

	Texture3D() = default;
	Texture3D(const Texture3D &rt) = delete;
	Texture3D(Texture3D &&rt);
	~Texture3D();
	Texture3D &operator=(Texture3D &&rt);

	inline bool create(const vec3u &texsize, Type type) { return create(texsize.x, texsize.y, texsize.z, type); }
	bool create(int width, int height, int depth, Type type);
	void cleanup();

	vec3i size = 0;
	dxptr<ID3D11Texture3D> texture = nullptr;
	dxptr<ID3D11UnorderedAccessView> uav = nullptr;
	dxptr<ID3D11ShaderResourceView> srv = nullptr;
};

struct RenderTexture {
	RenderTexture() = default;
	RenderTexture(const RenderTexture &rt) = delete;
	RenderTexture(RenderTexture &&rt);
	~RenderTexture();
	RenderTexture &operator=(RenderTexture &&rt);

	bool create(int width, int height);
	bool createFromBackbuffer();
	bool resize(int new_width, int new_height);
	void cleanup();

	void bind(ID3D11DepthStencilView *dsv = nullptr);
	void clear(Colour colour, ID3D11DepthStencilView *dsv = nullptr);

	bool takeScreenshot(const char *base_name = "screenshot");

	vec2i size;
	dxptr<ID3D11Texture2D> texture = nullptr;
	dxptr<ID3D11RenderTargetView> view = nullptr;
	dxptr<ID3D11ShaderResourceView> resource = nullptr;
};