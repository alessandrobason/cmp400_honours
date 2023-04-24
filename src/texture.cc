#include "texture.h"

#include <d3d11.h>
#include <stb_image.h>
#include <stb_image_write.h>

#include "system.h"
#include "utils.h"
#include "tracelog.h"

/* ==========================================
   =============== TEXTURE 2D ===============
   ========================================== */


Texture2D::Texture2D(Texture2D &&rt) {
	*this = mem::move(rt);
}

Texture2D::~Texture2D() {
	cleanup();
}

Texture2D &Texture2D::operator=(Texture2D &&rt) {
	if (this != &rt) {
		cleanup();
		mem::copy(*this, rt);
		mem::zero(rt);
	}
	return *this;
}

bool Texture2D::load(const char *filename) {
	cleanup();

	int channels = 0;
	unsigned char *data = stbi_load(filename, &size.x, &size.y, &channels, STBI_rgb_alpha);
	if (!data) {
		return false;
	}

	D3D11_TEXTURE2D_DESC desc;
	mem::zero(desc);
	desc.Width = size.x;
	desc.Height = size.y;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_IMMUTABLE;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA data_desc;
	mem::zero(data_desc);
	data_desc.SysMemPitch = size.x * STBI_rgb_alpha;
	data_desc.pSysMem = data;

	HRESULT hr = gfx::device->CreateTexture2D(&desc, &data_desc, &texture);
	stbi_image_free(data);
	if (FAILED(hr)) {
		err("couldn't create 2D texture");
		return false;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
	mem::zero(srv_desc);
	srv_desc.Format = desc.Format;
	srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srv_desc.Texture2D.MipLevels = 1;

	hr = gfx::device->CreateShaderResourceView(texture, &srv_desc, &srv);
	if (FAILED(hr)) {
		err("couldn't create 2D texture's SRV");
		cleanup();
		return false;
	}

	return true;
}

bool Texture2D::loadHDR(const char *filename) {
	cleanup();

	int channels = 0;
	float *data = stbi_loadf(filename, &size.x, &size.y, &channels, 4);
	if (!data) {
		return false;
	}

	D3D11_TEXTURE2D_DESC desc;
	mem::zero(desc);
	desc.Width = size.x;
	desc.Height = size.y;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_IMMUTABLE;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA data_desc;
	mem::zero(data_desc);
	data_desc.SysMemPitch = size.x * 4 * sizeof(float);
	data_desc.pSysMem = data;

	HRESULT hr = gfx::device->CreateTexture2D(&desc, &data_desc, &texture);
	stbi_image_free(data);
	if (FAILED(hr)) {
		err("couldn't create 2D texture");
		return false;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
	mem::zero(srv_desc);
	srv_desc.Format = desc.Format;
	srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srv_desc.Texture2D.MipLevels = 1;

	hr = gfx::device->CreateShaderResourceView(texture, &srv_desc, &srv);
	if (FAILED(hr)) {
		err("couldn't create 2D texture's SRV");
		cleanup();
		return false;
	}

	return true;
}

void Texture2D::cleanup() {
	texture.destroy();
	srv.destroy();
}

/* ==========================================
   =============== TEXTURE 3D ===============
   ========================================== */

static DXGI_FORMAT to_dx_format[(int)Texture3D::Type::count] = {
	DXGI_FORMAT_R8_UINT,         // uint8
	DXGI_FORMAT_R16_UINT,        // uint16
	DXGI_FORMAT_R32_UINT,        // uint32
	DXGI_FORMAT_R8_SINT,         // sint8
	DXGI_FORMAT_R16_SINT,        // sint16
	DXGI_FORMAT_R32_SINT,        // sint32
	DXGI_FORMAT_R16_FLOAT,       // float16
	DXGI_FORMAT_R32_FLOAT,       // float32
	DXGI_FORMAT_R16G16_UINT,     // r16g16_uint
	DXGI_FORMAT_R11G11B10_FLOAT, // r11g11b10_float
};

static size_t type_to_size[(int)Texture3D::Type::count] = {
	sizeof(uint8_t),  // uint8
	sizeof(uint16_t), // uint16
	sizeof(uint32_t), // uint32
	sizeof(int8_t),   // sint8
	sizeof(int16_t),  // sint16
	sizeof(int32_t),  // sint32
	sizeof(uint16_t), // float16
	sizeof(float),    // float32
	sizeof(uint32_t), // r16g16_uint
	sizeof(uint32_t), // r11g11b10_float
};

static Texture3D::Type dxToType(DXGI_FORMAT format) {
	using Type = Texture3D::Type;
	switch (format) {
		case DXGI_FORMAT_R8_UINT:         return Type::uint8;
		case DXGI_FORMAT_R16_UINT:        return Type::uint16;
		case DXGI_FORMAT_R32_UINT:        return Type::uint32;
		case DXGI_FORMAT_R8_SINT:         return Type::sint8;
		case DXGI_FORMAT_R16_SINT:        return Type::sint16;
		case DXGI_FORMAT_R32_SINT:        return Type::sint32;
		case DXGI_FORMAT_R16_FLOAT:       return Type::float16;
		case DXGI_FORMAT_R32_FLOAT:       return Type::float32;
		case DXGI_FORMAT_R16G16_UINT:     return Type::r16g16_uint;
		case DXGI_FORMAT_R11G11B10_FLOAT: return Type::r11g11b10_float;
	}
	return Type::count;
}

Texture3D::Texture3D(Texture3D &&rt) {
	*this = mem::move(rt);
}

Texture3D::~Texture3D() {
	cleanup();
}

Texture3D &Texture3D::operator=(Texture3D &&rt) {
	if (this != &rt) {
		cleanup();
		mem::copy(*this, rt);
		mem::zero(rt);
	}

	return *this;
}

bool Texture3D::create(const vec3u &texsize, Type type, const void *initial_data) {
	return create(texsize.x, texsize.y, texsize.z, type, initial_data);
}

bool Texture3D::create(int width, int height, int depth, Type type, const void *initial_data) {
	cleanup();
	
	size = vec3i(width, height, depth);

	D3D11_TEXTURE3D_DESC desc;
	mem::zero(desc);
	desc.Width = (UINT)width;
	desc.Height = (UINT)height;
	desc.Depth = (UINT)depth;
	desc.MipLevels = 1;
	desc.Format = to_dx_format[(int)type];
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	HRESULT hr = E_FAIL;

	if (initial_data) {
		size_t type_size = type_to_size[(int)type];
		D3D11_SUBRESOURCE_DATA data;
		mem::zero(data);
		data.pSysMem = initial_data;
		data.SysMemPitch = size.x * type_size;
		data.SysMemSlicePitch = size.x * size.y * type_size;
		hr = gfx::device->CreateTexture3D(&desc, &data, &texture);
	}
	else {
		hr = gfx::device->CreateTexture3D(&desc, nullptr, &texture);
	}

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

bool Texture3D::load(const char *filename) {
	file::fp fp = filename;
	if (!fp) {
		err("couldn't open file (%s)", filename);
		return false;
	}

	Type type;
	char header[5];
	fp.read(header);

	if (memcmp(header, "tex3d", sizeof(header)) != 0) {
		err("file (%s) is not a Texture3D bin file, the header should be \"tex3d\" but instead is \"%.5s\"", header);
		return false;
	}

	fp.read(size);
	fp.read(type);

	size_t type_size = type_to_size[(int)type];
	size_t data_len = size.x * size.y * size.z * type_size;
	mem::ptr<uint8_t[]> data = mem::ptr<uint8_t[]>::make(data_len);

	fp.read(data.get(), data_len);

	create(size, type, data.get());

	return true;
}

bool Texture3D::save(const char *filename) {
	if (file::exists(filename)) {
		err("trying to save a Texture3D but file (%s) already exists", filename);
		return false;
	}

	file::fp fp(filename, "wb");
	if (!fp) {
		err("couldn't open file (%s)", filename);
		return false;
	}

	// create a temporary texture that we can read from
	D3D11_TEXTURE3D_DESC desc;
	texture->GetDesc(&desc);
	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

	Type type = dxToType(desc.Format);

	dxptr<ID3D11Texture3D> temp = nullptr;
	HRESULT hr = gfx::device->CreateTexture3D(&desc, nullptr, &temp);
	if (FAILED(hr)) {
		err("couldn't create temporary texture3D");
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

	// write the data to the file

	char header[] = "tex3d";
	fp.write(header, sizeof(header) - 1);
	fp.write(size);
	fp.write(type);
	fp.write(mapped.pData, size.x * size.y * size.z * type_to_size[(int)type]);

	gfx::context->Unmap(temp, 0);

	return true;
}

void Texture3D::cleanup() {
	texture.destroy();
	uav.destroy();
	srv.destroy();
}


/* ==========================================
   ============= RENDER TEXTURE =============
   ========================================== */

RenderTexture::RenderTexture(RenderTexture &&rt) {
	*this = mem::move(rt);
}

RenderTexture::~RenderTexture() {
	cleanup();
}

RenderTexture &RenderTexture::operator=(RenderTexture &&rt) {
	if (this != &rt) {
		cleanup();
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
	td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
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
		
	HRESULT hr = gfx::swapchain->GetBuffer(0, IID_PPV_ARGS(&texture));
	if (FAILED(hr)) {
		err("couldn't get backbuffer");
		return false;
	}

	hr = gfx::device->CreateRenderTargetView(texture, nullptr, &view);
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
	texture.destroy();
	view.destroy();
	resource.destroy();
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

extern void addMessageToWidget(LogLevel severity, const char* message);

bool RenderTexture::takeScreenshot(const char* base_name) {
	// create a temporary texture that we can read from
	D3D11_TEXTURE2D_DESC td;
	mem::zero(td);
	texture->GetDesc(&td);
	td.Usage = D3D11_USAGE_STAGING;
	td.BindFlags = 0;
	td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

	dxptr<ID3D11Texture2D> temp = nullptr;
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

	char base_fmt[64];
	str::formatBuf(base_fmt, sizeof(base_fmt), "%s_%%03d.png", base_name);
	mem::ptr<char[]> name = file::findFirstAvailable("screenshots", base_fmt);
	bool success = stbi_write_png(name.get(), size.x, size.y, 4, mapped.pData, mapped.RowPitch);

	if (success) {
		info("saved screenshot as %s", name.get());
		addMessageToWidget(LogLevel::Info, str::format("saved screenshot as %s", name.get()));
	}

	gfx::context->Unmap(temp, 0);

	return success;
}