#include "texture.h"

#include <string>

#include <d3d11.h>
#include <stb_image_write.h>

#include "system.h"
#include "utils.h"
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
			mem::copy(*this, rt);
			mem::zero(rt);
		}

		return *this;
	}

	bool RenderTexture::create(int width, int height) {
		cleanup();

		size = { width, height };
		
		D3D11_TEXTURE2D_DESC td;
		mem::zero(td);
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
		mem::zero(vd);
		vd.Format = td.Format;
		vd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

		hr = gfx::device->CreateRenderTargetView(texture, &vd, &view);
		if (FAILED(hr)) {
			err("couldn't create RTV");
			return false;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC sd;
		mem::zero(sd);
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

		size = win::getSize();

		return true;
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
		D3D11_VIEWPORT vp;
		mem::zero(vp);
		vp.Width = (float)size.x;
		vp.Height = (float)size.y;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		vp.TopLeftX = vp.TopLeftY = 0;
		gfx::context->RSSetViewports(1, &vp);
		gfx::context->OMSetRenderTargets(1, &view, dsv);
	}

	void RenderTexture::clear(Colour colour, ID3D11DepthStencilView *dsv) {
		gfx::context->ClearRenderTargetView(view, colour.data);
		if (dsv) {
			gfx::context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH, 1.f, 0);
		}
	}

	bool RenderTexture::takeScreenshot(const char* base_name) {
		// create a temporary texture that we can read from
		D3D11_TEXTURE2D_DESC td;
		mem::zero(td);
		texture->GetDesc(&td);
		td.Usage = D3D11_USAGE_STAGING;
		td.BindFlags = 0;
		td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

		ID3D11Texture2D *temp = nullptr;
		HRESULT hr = gfx::device->CreateTexture2D(&td, nullptr, &temp);
		if (FAILED(hr)) {
			err("couldn't create temporary texture2D");
			return false;
		}

		// copy the texture to the temporary texture
		gfx::context->CopyResource(temp, texture);

		// map texture data
		D3D11_MAPPED_SUBRESOURCE mapped;
		hr = gfx::context->Map(temp, 0, D3D11_MAP_READ, 0, &mapped);
		if (FAILED(hr)) {
			err("couldn't map temporary texture2D");
			return false;
		}

		// find a name that hasn't been used yet
		char name[255];
		mem::zero(name);
		int count = 0;
		while (
			file::exists(
				str::formatBuf(name, sizeof(name), "screenshots/%s_%.3d.png", base_name, count)
			)
		) {
			count++;
		}

		bool success = stbi_write_png(name, size.x, size.y, 4, mapped.pData, mapped.RowPitch);

		if (success) {
			info("saved screenshot as %s", name);
		}

		gfx::context->Unmap(temp, 0);

		return success;
	}


	Texture3D::Texture3D(Texture3D &&rt) {
		*this = std::move(rt);
	}

	Texture3D::~Texture3D() {
		cleanup();
	}

	Texture3D &Texture3D::operator=(Texture3D &&rt) {
		if (this != &rt) {
			mem::copy(*this, rt);
			mem::zero(rt);
		}

		return *this;
	}

	bool Texture3D::create(int width, int height, int depth) {
		D3D11_TEXTURE3D_DESC desc;
		mem::zero(desc);
		desc.Width  = (UINT)width;
		desc.Height = (UINT)height;
		desc.Depth  = (UINT)depth;
		desc.MipLevels = 1;
		//desc.Format = DXGI_FORMAT_R8_SINT;
		desc.Format = DXGI_FORMAT_R32_FLOAT;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		HRESULT hr = device->CreateTexture3D(&desc, nullptr, &texture);
		if (FAILED(hr)) {
			err("couldn't create 3D texture");
			return false;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
		mem::zero(srv_desc);
		srv_desc.Format = desc.Format;
		srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
		srv_desc.Texture2D.MostDetailedMip = 0;
		srv_desc.Texture2D.MipLevels = 1;
		hr = gfx::device->CreateShaderResourceView(texture, &srv_desc, &srv);
		if (FAILED(hr)) {
			err("couldn't create srv");
			return false;
		}

		D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
		mem::zero(uav_desc);
		uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
		uav_desc.Format = desc.Format;
		uav_desc.Texture3D.WSize = desc.Depth;
		hr = gfx::device->CreateUnorderedAccessView(texture, &uav_desc, &uav);
		if (FAILED(hr)) {
			err("couldn't create uav");
			return false;
		}

		return true;
	}

	void Texture3D::cleanup() {
		SAFE_RELEASE(texture);
	}
} // namespace gfx
