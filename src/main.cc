#include <stdio.h>
#include <iostream>

#include "system.h"
#include "input.h"
#include "maths.h"
#include "options.h"
#include "tracelog.h"
#include "widgets.h"
#include "shader.h"
#include "mesh.h"
#include "macros.h"
#include "timer.h"
#include "shape_builder.h"

#include <imgui.h>

#include <d3d11.h>
#include "utils.h"

//#include <array>
#define USE_STRUCTURED_BUFFERS 1
#define TEST_DOUBLE 1

#if 0
struct BufType {
	int i;
	float f;
#ifdef TEST_DOUBLE
	double d;
#endif
};

#pragma comment( lib, "dxguid.lib") 

constexpr int NUM_ELEMENTS = 100;
BufType buf0[NUM_ELEMENTS];
BufType buf1[NUM_ELEMENTS];
ID3D11Buffer *gfx_buf0 = nullptr;
ID3D11Buffer *gfx_buf1 = nullptr;
ID3D11Buffer *gfx_buf_res = nullptr;
ID3D11ShaderResourceView *gfx_buf0_srv = nullptr;
ID3D11ShaderResourceView *gfx_buf1_srv = nullptr;
ID3D11UnorderedAccessView *gfx_buf_res_uav = nullptr;
#endif

constexpr vec2u tex_size = { 1920, 1080 };
ID3D11Texture2D *gfx_tex = nullptr;
ID3D11ShaderResourceView *gfx_tex_srv = nullptr;
ID3D11UnorderedAccessView *gfx_tex_uav = nullptr;
bool is_dirty = true;

//ShapeBuilder builder;

HRESULT createStructuredBuffer(ID3D11Device *dev, UINT elem_sz, UINT count, void *data, ID3D11Buffer **out);
HRESULT createRawBuffer(ID3D11Device *dev, UINT size, void *data, ID3D11Buffer **out);
HRESULT createBufferSRV(ID3D11Device *dev, ID3D11Buffer *buf, ID3D11ShaderResourceView **out);
HRESULT createBufferUAV(ID3D11Device *dev, ID3D11Buffer *buf, ID3D11UnorderedAccessView **out);
ID3D11Buffer *createAndCopyBuf(ID3D11Device *dev, ID3D11DeviceContext *ctx, ID3D11Buffer *buf);
//void runComputeShader(ID3D11DeviceContext *ctx, ID3D11ComputeShader *shader, Slice<ID3D11ShaderResourceView *> views, ID3D11Buffer *cbcs, void *cs_data, DWORD num_data_bytes, ID3D11UnorderedAccessView *unordered_access_view, vec3u threads);

struct PSShaderData {
	vec3 cam_up;
	float time;
	vec3 cam_fwd;
	float img_width;
	vec3 cam_right;
	float img_height;
	vec3 cam_pos;
	float unused__2;
};

struct Camera {
	vec3 pos   = vec3(0, 0, -7);
	vec3 fwd   = vec3(0, 0, 1);
	vec3 right = vec3(1, 0, 0);
	vec3 up    = vec3(0, 1, 0);
	void update(const vec3 &newpos, const vec3 &newdir) {
		pos = newpos;
		fwd = newdir;
		right = norm(cross(fwd, up));
		// up = cross(right, fwd);
	}
};

void gfxErrorExit() {
	gfx::logD3D11messages();
	exit(1);
}

int main() {
	win::create("hello world", 800, 600);

	debug("debug");
	info("info");
	warn("warn");
	err("err");

	debug("err");
	info("debug");
	warn("info");
	err("warn");

	debug("warn");
	info("err");
	warn("debug");
	err("info");

	debug("info");
	info("warn");
	warn("err");
	err("debug");

	gfx::Texture3D texture3d;
	if (!texture3d.create(64 * 3, 64 * 3, 32 * 3)) {
		gfxErrorExit();
	}

	DynamicShader sh;
	if (!sh.init("base_vs", "base_ps", "brush_grid")) {
		gfxErrorExit();
	}
	if (!sh.shader.addSampler()) {
		gfxErrorExit();
	}
	int buf_ind = sh.shader.addBuffer<PSShaderData>(Usage::Dynamic);
	Camera cam;

	if (Buffer *buf = sh.shader.getBuffer(buf_ind)) {
		if (PSShaderData *data = buf->map<PSShaderData>()) {
			data->cam_pos = cam.pos;
			data->cam_fwd = cam.fwd;
			data->cam_right = cam.right;
			data->cam_up = cam.up;
			data->img_height = (float)tex_size.x;
			data->img_width = (float)tex_size.y;
			data->time = win::timeSinceStart();
			buf->unmapPS();
		}
	}

	Vertex verts[] = {
		{ vec3(-1, -1, 0), vec2(0, 0) },
		{ vec3(3, -1, 0), vec2(2, 0) },
		{ vec3(-1,  3, 0), vec2(0, 2) },
	};

	Index indices[] = {
		0, 2, 1,
	};

	Mesh m;
	if (!m.create(verts, indices)) {
		gfxErrorExit();
	}

	sh.shader.dispatch({ 1, 1, 1 }, {}, { texture3d.uav });
	sh.shader.setSRV(ShaderType::Fragment, { texture3d.srv });

	while (win::isOpen()) {
		sh.poll();

#if 1
		if (Buffer *buf = sh.shader.getBuffer(buf_ind)) {
			if (PSShaderData *data = buf->map<PSShaderData>()) {
				data->cam_pos    = cam.pos;
				data->cam_fwd    = cam.fwd;
				data->cam_right  = cam.right;
				data->cam_up     = cam.up;
				data->img_height = (float)tex_size.x;
				data->img_width  = (float)tex_size.y;
				data->time       = win::timeSinceStart();
				buf->unmapPS();
				is_dirty = true;
			}
		}
#endif

		is_dirty |= sh.hasUpdated();

		if (sh.hasUpdated()) {
			is_dirty = true;
			if (sh.getChanged() & ShaderType::Compute) {
				sh.shader.setSRV(ShaderType::Fragment, { nullptr });
				sh.shader.dispatch({ 1, 1, 1 }, {}, { texture3d.uav });
				sh.shader.setSRV(ShaderType::Fragment, { texture3d.srv });
			}
		}

		gfx::begin(Colour::dark_grey);

		if (is_dirty || !Options::get().lazy_render) {
			if (Options::get().lazy_render) {
				info("rendering again");
			}
			gfx::main_rtv.clear(Colour::dark_grey);
			gfx::main_rtv.bind();
			sh.shader.bind();
				m.render();
			sh.shader.unbind();
		}
		
		gfx::imgui_rtv.bind();
			fpsWidget();
			mainTargetWidget();
			ImGui::ShowDemoWindow();
			drawLogger();
		gfx::end();

		if (ImGui::IsKeyPressed(ImGuiKey_P)) {
			gfx::imgui_rtv.takeScreenshot();
		}

		gfx::logD3D11messages();

		is_dirty = false;
	}

	win::cleanup();
}

#if 0
int main() {
	win::create("hello world", 800, 600);

	builder.init();

	// push stack so stuff auto-cleans itself
	{
		Vertex verts[] = {
			{ vec3(-1, -1, 0), vec2(0, 0) },
			{ vec3( 3, -1, 0), vec2(2, 0) },
			{ vec3(-1,  3, 0), vec2(0, 2) },
		};

		Index indices[] = {
			0, 2, 1,
		};

		Mesh m;
		if (!m.create(verts, indices)) {
			gfxErrorExit();
		}

		info("creating output texture");

		D3D11_TEXTURE2D_DESC tex_desc;
		mem::zero(tex_desc);
		tex_desc.Width = tex_size.x;
		tex_desc.Height = tex_size.y;
		tex_desc.MipLevels = 1;
		tex_desc.ArraySize = 1;
		tex_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		tex_desc.SampleDesc.Count = 1;
		tex_desc.Usage = D3D11_USAGE_DEFAULT;
		tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		gfx::device->CreateTexture2D(&tex_desc, nullptr, &gfx_tex);

		D3D11_SHADER_RESOURCE_VIEW_DESC texsrv_desc;
		mem::zero(texsrv_desc);
		texsrv_desc.Format = tex_desc.Format;
		texsrv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		texsrv_desc.Texture2D.MostDetailedMip = 0;
		texsrv_desc.Texture2D.MipLevels = 1;
		gfx::device->CreateShaderResourceView(gfx_tex, &texsrv_desc, &gfx_tex_srv);

		D3D11_UNORDERED_ACCESS_VIEW_DESC texuav_desc;
		mem::zero(texuav_desc);
		texuav_desc.Format = DXGI_FORMAT_UNKNOWN;
		texuav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		gfx::device->CreateUnorderedAccessView(gfx_tex, &texuav_desc, &gfx_tex_uav);

		info("completed");

#if 0
		info("creating buffers and filling them with initial data");

		for (int i = 0; i < NUM_ELEMENTS; ++i) {
			buf0[i].i = i;
			buf0[i].f = (float)i;
#ifdef TEST_DOUBLE
			buf0[i].d = (double)i;
#endif

			buf1[i].i = i;
			buf1[i].f = (float)i;
#ifdef TEST_DOUBLE
			buf1[i].d = (double)i;
#endif
		}

#ifdef USE_STRUCTURED_BUFFERS
		createStructuredBuffer(gfx::device, sizeof(BufType), NUM_ELEMENTS, &buf0[0], &gfx_buf0);
		createStructuredBuffer(gfx::device, sizeof(BufType), NUM_ELEMENTS, &buf1[0], &gfx_buf1);
		createStructuredBuffer(gfx::device, sizeof(BufType), NUM_ELEMENTS, nullptr, &gfx_buf_res);
#else
		createRawBuffer(gfx::device, NUM_ELEMENTS * sizeof(BufType), &buf0[0], &gfx_buf0);
		createRawBuffer(gfx::device, NUM_ELEMENTS * sizeof(BufType), &buf1[0], &gfx_buf1);
		createRawBuffer(gfx::device, NUM_ELEMENTS * sizeof(BufType), nullptr, &gfx_buf_res);
#endif

#if defined(_DEBUG) || defined(PROFILE)
		if (gfx_buf0)
			gfx_buf0->SetPrivateData(WKPDID_D3DDebugObjectName, sizeof("Buffer0") - 1, "Buffer0");
		if (gfx_buf1)
			gfx_buf1->SetPrivateData(WKPDID_D3DDebugObjectName, sizeof("Buffer1") - 1, "Buffer1");
		if (gfx_buf_res)
			gfx_buf_res->SetPrivateData(WKPDID_D3DDebugObjectName, sizeof("Result") - 1, "Result");
#endif

		info("done");

		info("creating buffer views");

		createBufferSRV(gfx::device, gfx_buf0, &gfx_buf0_srv);
		createBufferSRV(gfx::device, gfx_buf1, &gfx_buf1_srv);
		createBufferUAV(gfx::device, gfx_buf_res, &gfx_buf_res_uav);

#if defined(_DEBUG) || defined(PROFILE)
		if (gfx_buf0_srv)
			gfx_buf0_srv->SetPrivateData(WKPDID_D3DDebugObjectName, sizeof("Buffer0 SRV") - 1, "Buffer0 SRV");
		if (gfx_buf1_srv)
			gfx_buf1_srv->SetPrivateData(WKPDID_D3DDebugObjectName, sizeof("Buffer1 SRV") - 1, "Buffer1 SRV");
		if (gfx_buf_res_uav)
			gfx_buf_res_uav->SetPrivateData(WKPDID_D3DDebugObjectName, sizeof("Result UAV") - 1, "Result UAV");
#endif

		info("done");
#endif

		Shader sh;
		// if (!sh.create("base_vs", "base_ps")) {
		if (!sh.create("test_comp")) {
			gfxErrorExit();
		}

#if 0
		if (0) {
			ID3D11Buffer *debugbuf = createAndCopyBuf(gfx::device, gfx::context, gfx_buf_res);
			D3D11_MAPPED_SUBRESOURCE map_sub;
			BufType *p;
			gfx::context->Map(debugbuf, 0, D3D11_MAP_READ, 0, &map_sub);


			// Set a break point here and put down the expression "p, 1024" in your watch window to see what has been written out by our CS
			// This is also a common trick to debug CS programs.
			p = (BufType *)map_sub.pData;

			// Verify that if Compute Shader has done right
			info("verifying against CPU result");
			bool bSuccess = true;
			for (int i = 0; i < NUM_ELEMENTS; ++i)
				if ((p[i].i != buf0[i].i + buf1[i].i)
					|| (p[i].f != buf0[i].f + buf1[i].f)
#ifdef TEST_DOUBLE
					|| (p[i].d != buf0[i].d + buf1[i].d)
#endif
				) {
					info("failure");
					bSuccess = false;

					break;
				}
			if (bSuccess)
				info("succeeded");

			gfx::context->Unmap(debugbuf, 0);

			SAFE_RELEASE(debugbuf);
		}
#endif

		builder.addShape({ 0, 0, 0 }, Shape::Sphere);

		while (win::isOpen()) {
			win::poll();

			static vec3u threads = { 1, 1, 1 };
			if (isKeyPressed(KEY_I)) threads.x++;
			if (isKeyPressed(KEY_O)) threads.y++;
			if (isKeyPressed(KEY_P)) threads.z++;

			threads.x = math::max(threads.x, 1u);
			threads.y = math::max(threads.y, 1u);
			threads.z = math::max(threads.z, 1u);
			//math::clamp(threads, { 0, 0, 0 }, { 1, 1, 1 });

			gfx::context->CSSetShader(sh.compute, nullptr, 0);
				builder.bind();
				gfx::context->CSSetUnorderedAccessViews(0, 1, &gfx_tex_uav, nullptr);
				gfx::context->Dispatch(threads.x, threads.y, threads.z);

				ID3D11UnorderedAccessView *uav_nullptr[1] = { nullptr };
				gfx::context->CSSetUnorderedAccessViews(0, 1, uav_nullptr, nullptr);
				builder.unbind();
			gfx::context->CSSetShader(nullptr, nullptr, 0);
			// runComputeShader(gfx::context, sh.compute, {}, nullptr, nullptr, 0, gfx_tex_uav, threads);

			gfx::begin(Colour::dark_grey);
				//gfx::main_rtv.bind();
				//	sh.bind();
				//		m.render();
				//	sh.unbind();
				gfx::imgui_rtv.bind();
					fpsWidget();
					mainTargetWidget(tex_size, gfx_tex_srv);
					//ImGui::Image((ImTextureID)gfx_tex_srv, (vec2)tex_size);
					//ImGui::End();
					ImGui::ShowDemoWindow();
				ImGui::Begin("Debug");
				
				imInputUint3("threads", (unsigned int *)&threads);
				ImGui::End();
			gfx::end();

			if (ImGui::IsKeyPressed(ImGuiKey_P)) {
				gfx::imgui_rtv.takeScreenshot();
			}

			gfx::logD3D11messages();
		}

		info("Cleaning up\n");
#if 0
		SAFE_RELEASE(gfx_buf0_srv);
		SAFE_RELEASE(gfx_buf1_srv);
		SAFE_RELEASE(gfx_buf_res_uav);
		SAFE_RELEASE(gfx_buf0);
		SAFE_RELEASE(gfx_buf1);
		SAFE_RELEASE(gfx_buf_res);
#endif

		SAFE_RELEASE(gfx_tex);
		SAFE_RELEASE(gfx_tex_srv);
		SAFE_RELEASE(gfx_tex_uav);

#if 0
		while (win::isOpen()) {
			win::poll();

			//sh.update(win::timeSinceStart());

			gfx::begin(Colour::dark_grey);
				gfx::main_rtv.bind();
					sh.bind();
						m.render();
					sh.unbind();
				gfx::imgui_rtv.bind();
					fpsWidget();
					mainTargetWidget();
					ImGui::ShowDemoWindow();
			gfx::end();

#ifndef NDEBUG
			// take a one screenshot every time we open the application, this way we can have
			// a history of the development
			static OnceClock screenshot_clock;
			if (screenshot_clock.after(0.1f)) {
				gfx::imgui_rtv.takeScreenshot("automatic_screen");
			}
#endif

			if (ImGui::IsKeyPressed(ImGuiKey_P)) {
				gfx::imgui_rtv.takeScreenshot();
			}

			gfx::logD3D11messages();
		}
#endif
	}

	builder.cleanup();

	win::cleanup();
}

#endif

HRESULT createStructuredBuffer(ID3D11Device *dev, UINT elem_sz, UINT count, void *data, ID3D11Buffer **out) {
	*out = nullptr;

	D3D11_BUFFER_DESC desc;
	mem::zero(desc);
	desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	desc.ByteWidth = elem_sz * count;
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	desc.StructureByteStride = elem_sz;

	if (data) {
		D3D11_SUBRESOURCE_DATA init_data;
		mem::zero(init_data);
		init_data.pSysMem = data;
		return dev->CreateBuffer(&desc, &init_data, out);
	}
	else {
		return dev->CreateBuffer(&desc, nullptr, out);
	}
}

HRESULT createRawBuffer(ID3D11Device *dev, UINT size, void *data, ID3D11Buffer **out) {
	*out = nullptr;

	D3D11_BUFFER_DESC desc;
	mem::zero(desc);
	desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_INDEX_BUFFER | D3D11_BIND_VERTEX_BUFFER;
	desc.ByteWidth = size;
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;

	if (data) {
		D3D11_SUBRESOURCE_DATA init_data;
		mem::zero(init_data);
		init_data.pSysMem = data;
		return dev->CreateBuffer(&desc, &init_data, out);
	}
	else {
		return dev->CreateBuffer(&desc, nullptr, out);
	}
}

HRESULT createBufferSRV(ID3D11Device *dev, ID3D11Buffer *buf, ID3D11ShaderResourceView **out) {
	D3D11_BUFFER_DESC bufdesc;
	mem::zero(bufdesc);
	buf->GetDesc(&bufdesc);

	D3D11_SHADER_RESOURCE_VIEW_DESC desc;
	mem::zero(desc);
	desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
	desc.BufferEx.FirstElement = 0;

	if (bufdesc.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS) {
		// raw buffer

		desc.Format = DXGI_FORMAT_R32_TYPELESS;
		desc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
		desc.BufferEx.NumElements = bufdesc.ByteWidth / 4;
	}
	else if (bufdesc.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED) {
		// structured buffer

		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.BufferEx.NumElements = bufdesc.ByteWidth / bufdesc.StructureByteStride;
	}
	else {
		return E_INVALIDARG;
	}

	return dev->CreateShaderResourceView(buf, &desc, out);
}

HRESULT createBufferUAV(ID3D11Device *dev, ID3D11Buffer *buf, ID3D11UnorderedAccessView **out) {
	D3D11_BUFFER_DESC bufdesc;
	mem::zero(bufdesc);
	buf->GetDesc(&bufdesc);

	D3D11_UNORDERED_ACCESS_VIEW_DESC desc = {};
	desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	desc.Buffer.FirstElement = 0;

	if (bufdesc.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS) {
		// raw buffer

		desc.Format = DXGI_FORMAT_R32_TYPELESS; // Format must be DXGI_FORMAT_R32_TYPELESS, when creating Raw Unordered Access View
		desc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
		desc.Buffer.NumElements = bufdesc.ByteWidth / 4;
	}
	else if(bufdesc.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED) {
		// structured buffer

		desc.Format = DXGI_FORMAT_UNKNOWN;      // Format must be must be DXGI_FORMAT_UNKNOWN, when creating a View of a Structured Buffer
		desc.Buffer.NumElements = bufdesc.ByteWidth / bufdesc.StructureByteStride;
	}
	else {
		return E_INVALIDARG;
	}

	return dev->CreateUnorderedAccessView(buf, &desc, out);
}

ID3D11Buffer *createAndCopyBuf(ID3D11Device *dev, ID3D11DeviceContext *ctx, ID3D11Buffer *buf) {
	ID3D11Buffer *debugbuf = nullptr;

	D3D11_BUFFER_DESC desc = {};
	buf->GetDesc(&desc);
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.MiscFlags = 0;
	if (SUCCEEDED(dev->CreateBuffer(&desc, nullptr, &debugbuf))) {
#if 0 && defined(_DEBUG) || defined(PROFILE)
		debugbuf->SetPrivateData(WKPDID_D3DDebugObjectName, sizeof("Debug") - 1, "Debug");
#endif

		ctx->CopyResource(debugbuf, buf);
	}

	return debugbuf;
}

#if 0
void runComputeShader(
	ID3D11DeviceContext *ctx,
	ID3D11ComputeShader *shader,
	// Slice<ID3D11ShaderResourceView *> views,
	// ID3D11Buffer *cbcs,
	// void *cs_data,
	// DWORD num_data_bytes,
	ID3D11UnorderedAccessView *unordered_access_view,
	vec3u threads
) {
	ctx->CSSetShader(shader, nullptr, 0);
	//ctx->CSSetShaderResources(0, (UINT)views.len, views.data);
	//ctx->CSSetUnorderedAccessViews(0, 1, &unordered_access_view, nullptr);
	
	// if (cbcs && cs_data) {
	// 	D3D11_MAPPED_SUBRESOURCE map_sub;
	// 	ctx->Map(cbcs, 0, D3D11_MAP_WRITE_DISCARD, 0, &map_sub);
	// 	memcpy(map_sub.pData, cs_data, num_data_bytes);
	// 	ctx->Unmap(cbcs, 0);
	// 	ctx->CSSetConstantBuffers(0, 1, &cbcs);
	// }

	ctx->Dispatch(threads.x, threads.y, threads.z);
	
	ctx->CSSetShader(nullptr, nullptr, 0);

	// ID3D11UnorderedAccessView *UA_view_nullptr[1] = { nullptr };
	// ctx->CSSetUnorderedAccessViews(0, 1, UA_view_nullptr, nullptr);

	// ID3D11ShaderResourceView *SRV_nullptr[2] = { nullptr, nullptr };
	// ctx->CSSetShaderResources(0, 2, SRV_nullptr);

	// ID3D11Buffer *CB_nullptr[1] = { nullptr };
	// ctx->CSSetConstantBuffers(0, 1, CB_nullptr);
}
#endif