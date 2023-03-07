#pragma once

#include "d3d11_fwd.h"
#include "colours.h"

namespace gfx {
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

	struct Texture3D {
		Texture3D() = default;
		Texture3D(const Texture3D &rt) = delete;
		Texture3D(Texture3D &&rt);
		~Texture3D();
		Texture3D &operator=(Texture3D &&rt);

		inline bool create(const vec3u &texsize) { return create(texsize.x, texsize.y, texsize.z); }
		bool create(int width, int height, int depth);
		void cleanup();

		vec3i size = 0;
		dxptr<ID3D11Texture3D> texture = nullptr;
		dxptr<ID3D11UnorderedAccessView> uav = nullptr;
		dxptr<ID3D11ShaderResourceView> srv = nullptr;
	};
} // namespace gfx 