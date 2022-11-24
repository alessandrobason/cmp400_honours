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

		vec2i size;
		ID3D11Texture2D *texture = nullptr;
		ID3D11RenderTargetView *view = nullptr;
		ID3D11ShaderResourceView *resource = nullptr;
	};
} // namespace gfx 