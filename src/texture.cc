#include "texture.h"

#include <d3d11.h>

#include "system.h"
#include "str_utils.h"
#include "tracelog.h"
#include "macros.h"

namespace gfx {
	RenderTexture::RenderTexture(RenderTexture &&rt) {
		*this = std::move(rt);
	}

	RenderTexture::~RenderTexture() {
		cleanup();
	}

	RenderTexture &RenderTexture::operator=(RenderTexture &&rt) {
		if (this != &rt) {
			str::memcopy(*this, rt);
			str::memzero(rt);
		}

		return *this;
	}

	bool RenderTexture::create(int width, int height) {
		cleanup();

		size = { width, height };
		
		D3D11_TEXTURE2D_DESC td;
		str::memzero(td);
		td.Width = width;
		td.Height = height;
		td.MipLevels = 1;
		td.ArraySize = 1;
		td.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		td.SampleDesc.Count = 1;
		td.Usage = D3D11_USAGE_DEFAULT;
		td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		
		HRESULT hr = gfx::device->CreateTexture2D(&td, nullptr, &texture);
		if (FAILED(hr)) {
			err("couldn't create RTV's texture2D");
			return false;
		}

		D3D11_RENDER_TARGET_VIEW_DESC vd;
		str::memzero(vd);
		vd.Format = td.Format;
		vd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

		hr = gfx::device->CreateRenderTargetView(texture, &vd, &view);
		if (FAILED(hr)) {
			err("couldn't create RTV");
			return false;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC sd;
		str::memzero(sd);
		sd.Format = td.Format;
		sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		sd.Texture2D.MipLevels = 1;

		hr = gfx::device->CreateShaderResourceView(texture, &sd, &resource);
		if (FAILED(hr)) {
			err("couldn't create RTV's shader resource view");
			return false;
		}

		return true;
	}

	bool RenderTexture::createFromBackbuffer() {
		cleanup();
		
		HRESULT hr = swapchain->GetBuffer(0, IID_PPV_ARGS(&texture));
		if (FAILED(hr)) {
			err("couldn't get backbuffer");
			return false;
		}

		hr = device->CreateRenderTargetView(texture, nullptr, &view);
		if (FAILED(hr)) {
			err("couldn't create view from backbuffer's texture");;
			return false;
		}

		return false;
	}

	bool RenderTexture::resize(int new_width, int new_height) {
		cleanup();
		return create(new_width, new_height);
	}

	void RenderTexture::cleanup() {
		SAFE_RELEASE(texture);
		SAFE_RELEASE(view);
		SAFE_RELEASE(resource);
	}

	void RenderTexture::bind(ID3D11DepthStencilView *dsv) {
		gfx::context->OMSetRenderTargets(1, &view, dsv);
	}

	void RenderTexture::clear(Colour colour, ID3D11DepthStencilView *dsv) {
		gfx::context->ClearRenderTargetView(view, colour.data);
		if (dsv) {
			gfx::context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH, 1.f, 0);
		}
	}
} // namespace gfx
