#include <stdio.h>
//#include <iostream>

#include "system.h"
#include "input.h"
#include "maths.h"
#include "options.h"
#include "tracelog.h"
#include "widgets.h"
#include "macros.h"
#include "timer.h"
#include "gfx.h"
#include "shape_builder.h"
#include "camera.h"

#include <imgui.h>

#include <d3d11.h>
#include <functional>

#include "utils.h"

//#include <tracy/TracyD3D11.hpp>

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

bool is_dirty = true;
constexpr vec3u maintex_size = 512;
constexpr vec3u brush_size = 64;
constexpr vec3u threads = maintex_size / 8;

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

#if BRUSH_BUILDER
struct SDFData {
	vec3 tex_size;
	float padding__0;
};

struct SDFinfo {
	vec3i half_tex_size;
	unsigned int padding__0;
};

struct BrushData {
	vec3i brush_size;
	unsigned int operation;
	vec3i brush_pos;
	int unused__1;
};
#endif

enum class Operations : uint32_t {
	None               = 0,
	Union              = 1u << 1,
	Subtraction        = 1u << 2,
	// Intersection       = 1u << 3,
	Smooth             = 1u << 31,
	SmoothUnion        = Smooth | Union,
	SmoothSubtraction  = Smooth | Subtraction,
	// SmoothIntersection = Smooth | Intersection,
};

Operations &operator|=(Operations &a, Operations b) { a = (Operations)((uint32_t)a | (uint32_t)b); return a; }

struct VolumeTexData {
	vec3 volume_tex_size = maintex_size;
	int padding__0;
};

struct TexData {
	vec3 tex_size = maintex_size;
	float padding__0;
	vec3 tex_position = 0;
	float padding__1;
};

struct BrushData {
	vec3 brush_size;
	Operations operation;
	vec3 brush_position;
	float smooth_amount;
};

#if 0
struct Camera {
	vec3 pos   = vec3(0, 0, -100);
	vec3 fwd   = vec3(0, 0, 1);
	vec3 right = vec3(1, 0, 0);
	vec3 up    = vec3(0, 1, 0);

	void input() {
		const vec3 velocity = {
			(float)(isKeyDown(KEY_A) - isKeyDown(KEY_D)),
			(float)(isKeyDown(KEY_Q) - isKeyDown(KEY_E)),
			(float)(isKeyDown(KEY_W) - isKeyDown(KEY_S))
		};

		const float speed = 50.f * win::dt;
		pos += velocity * speed;

		vec3 lookat = pos + fwd;

		const vec3 rotation = {
			(float)(isKeyDown(KEY_LEFT) - isKeyDown(KEY_RIGHT)),
			(float)(isKeyDown(KEY_DOWN) - isKeyDown(KEY_UP)),
			0
		};

		lookat += rotation * win::dt;
		fwd = norm(lookat - pos);
		//fwd = norm(fwd + rotation * win::dt);
		right = norm(cross(fwd, up));
		up = cross(right, fwd);
	}

	void update(const vec3 &newpos, const vec3 &newdir) {
		pos = newpos;
		fwd = newdir;
		right = norm(cross(fwd, up));
		up = cross(right, fwd);
	}
};
#endif

void gfxErrorExit() {
	gfx::logD3D11messages();
	exit(1);
}

Mesh makeFullScreenTriangle() {
	Mesh::Vertex verts[] = {
		{ vec3(-1, -1, 0), vec2(0, 0) },
		{ vec3( 3, -1, 0), vec2(2, 0) },
		{ vec3(-1,  3, 0), vec2(0, 2) },
	};

	Mesh::Index indices[] = {
		0, 2, 1,
	};

	Mesh m;
	if (!m.create(verts, indices)) {
		gfxErrorExit();
	}

	return m;
}

int main() {
	win::create("Honours Project", 800, 600);
	assert(all(maintex_size % 8 == 0));
	assert(all(brush_size % 8 == 0));

	{
		Camera cam;
		Texture3D main_texture, brush;
		DynamicShader main_sh, sculpt, gen_brush, empty_texture;

		if (!main_texture.create(maintex_size)) gfxErrorExit();
		if (!brush.create(brush_size))          gfxErrorExit();

		if (!main_sh.init("base_vs", "base_ps", nullptr)) gfxErrorExit();
		main_sh.shader.addSampler();
		int shader_data_ind = main_sh.shader.addBuffer<PSShaderData>(Buffer::Usage::Dynamic);
		int tex_data_ind = main_sh.shader.addBuffer<TexData>(Buffer::Usage::Dynamic);

		if (!sculpt.init(nullptr, nullptr, "sculpt_cs")) gfxErrorExit();
		VolumeTexData volume_tex_data{};
		int volume_tex_data_ind = sculpt.shader.addBuffer<VolumeTexData>(Buffer::Usage::Immutable, &volume_tex_data);
		int brush_data_ind = sculpt.shader.addBuffer<BrushData>(Buffer::Usage::Dynamic);

		if (!gen_brush.init(nullptr, nullptr, "gen_brush_cs")) gfxErrorExit();

		if (!empty_texture.init(nullptr, nullptr, "empty_texture_cs")) gfxErrorExit();

		Mesh triangle = makeFullScreenTriangle();

		// generate brush
		gen_brush.shader.dispatch(brush_size / 8, {}, {}, { brush.uav });

		// fill texture
		empty_texture.shader.dispatch(threads, {}, {}, { main_texture.uav });

		GPUClock sculpt_clock("Sculpt");

		if (Buffer *buf = main_sh.shader.getBuffer(tex_data_ind)) {
			if (TexData *data = buf->map<TexData>(0)) {
				data->tex_size = maintex_size;
				data->tex_position = 0;
				buf->unmap();
			}
		}

		if (Buffer *buf = sculpt.shader.getBuffer(brush_data_ind)) {
			if (BrushData *data = buf->map<BrushData>(0)) {
				data->brush_position = vec3(0);
				data->brush_size = brush_size;
				data->operation = Operations::Union;
				buf->unmap();
			}
		}

		// sculpt_clock.start();
		// sculpt.shader.dispatch(threads, { volume_tex_data_ind, brush_data_ind }, { brush.srv }, { main_texture.uav });
		// sculpt_clock.end();

		while (win::isOpen()) {
			main_sh.poll();
			sculpt.poll();
			gen_brush.poll();

			if (sculpt_clock.isReady()) sculpt_clock.print();
			if (isKeyPressed(KEY_ESCAPE)) win::close();

			//cam.input();
			cam.update();

			if (main_sh.hasUpdated()) {
				is_dirty = true;
				if (main_sh.getChanged() & ShaderType::Compute) {
					// TODO run compute
				}
			}

			if (sculpt.hasUpdated()) {
				is_dirty = true;
				empty_texture.shader.dispatch(threads, {}, {}, { main_texture.uav });
				sculpt_clock.start();
				sculpt.shader.dispatch(threads, { volume_tex_data_ind, brush_data_ind }, { brush.srv }, { main_texture.uav });
				sculpt_clock.end();
			}

			gfx::begin();

			if (!Options::get().lazy_render || is_dirty) {
				if (Options::get().lazy_render) {
					info("(Lazy Rendering) rendering scene again");
				}

				if (Buffer *buf = main_sh.shader.getBuffer(shader_data_ind)) {
					if (PSShaderData *data = buf->map<PSShaderData>()) {
						data->cam_pos = cam.pos;
						data->cam_fwd = cam.fwd;
						data->cam_right = cam.right;
						//data->cam_fwd = norm(cam.target - cam.pos); // cam.fwd;
						//data->cam_right = norm(cross(data->cam_fwd, cam.up)); // cam.right;
						data->cam_up = cam.up;
						data->img_height = (float)tex_size.x;
						data->img_width = (float)tex_size.y;
						data->time = win::timeSinceStart();
						buf->unmap();
						buf->bind(ShaderType::Fragment);
						is_dirty = true;
					}
				}

				gfx::main_rtv.clear(Colour::dark_grey);
				gfx::main_rtv.bind();
				main_sh.shader.bind();
				main_sh.shader.bindBuffers(ShaderType::Fragment, { shader_data_ind, tex_data_ind });
				main_sh.shader.setSRV(ShaderType::Fragment, { main_texture.srv });
				triangle.render();
				main_sh.shader.setSRV(ShaderType::Fragment, { nullptr });
				main_sh.shader.unbindBuffers(ShaderType::Fragment, 2);
				//main_sh.shader.unbind();
			}

			gfx::imgui_rtv.bind();
			fpsWidget();
			mainTargetWidget();
			drawLogger();
			
			ImGui::Begin("New Shape");
			static vec3 newpos = norm(vec3(1)) * 25.f;
			static int cur_op = 0;
			static bool is_smooth = false;
			static float smooth_amount = 0.5f;
			// ImGui::Combo("Operation", &cur_op, "Union\0Subtraction\0Intersection\0Smooth Union\0Smooth Subtraction\0Smooth Intersection");
			ImGui::Combo("Operation", &cur_op, "Union\0Subtraction");
			ImGui::Checkbox("Is smooth", &is_smooth);
			if (is_smooth) {
				ImGui::SliderFloat("Smooth Amount", &smooth_amount, 0.f, 1.f);
			}
			ImGui::DragFloat3("Position", newpos.data, 1.f, -(maintex_size.x / 2.f), maintex_size.x / 2.f);

			static bool once = true;

			if (ImGui::Button("Add") || once) {
				once = false;
				static Operations int_to_oper[6] = {
					Operations::Union,
					Operations::Subtraction,
					// Operations::Intersection,
					// Operations::SmoothUnion,
					// Operations::SmoothSubtraction,
					// Operations::SmoothIntersection,
				};

				if (Buffer *buf = sculpt.shader.getBuffer(brush_data_ind)) {
					if (BrushData *data = buf->map<BrushData>(0)) {
						data->brush_position = newpos;
						data->brush_size = brush_size;
						data->operation = int_to_oper[cur_op];
						if (is_smooth) data->operation |= Operations::Smooth;
						data->smooth_amount = smooth_amount;
						buf->unmap();
					}
				}

				sculpt_clock.start();
				sculpt.shader.dispatch(threads, { volume_tex_data_ind, brush_data_ind }, { brush.srv }, { main_texture.uav });
				sculpt_clock.end();
			}
			
			ImGui::End();

			gfx::end();

			if (isKeyPressed(KEY_P)) {
				gfx::main_rtv.takeScreenshot();
			}

			gfx::logD3D11messages();

			is_dirty = false;
		}
	}
}

#if BRUSH_BUILDER
int main() {
	win::create("hello world", 800, 600);

	assert(all(maintex_size % 8 == 0));
	assert(all(brush_size % 8 == 0));

	{
		Camera cam;
		ShapeBuilder builder;
		builder.init();

		gfx::Texture3D main_texture, brush;

		if (!main_texture.create(maintex_size)) {
			gfxErrorExit();
		}
		if (!brush.create(brush_size)) {
			gfxErrorExit();
		}

		DynamicShader main_sh, add_brush, edit_sh;

		if (!main_sh.init("base_vs", "base_ps", "brush_grid")) {
			gfxErrorExit();
		}
		if (!main_sh.shader.addSampler()) {
			gfxErrorExit();
		}
		int buf_ind = main_sh.shader.addBuffer<PSShaderData>(Usage::Dynamic);
		// TODO this might never change, change usage!
		int sdfbuf_ind = main_sh.shader.addBuffer<SDFData>(Usage::Dynamic);

		if (Buffer *buf = main_sh.shader.getBuffer(buf_ind)) {
			if (PSShaderData *data = buf->map<PSShaderData>()) {
				data->cam_pos = cam.pos;
				data->cam_fwd = cam.fwd;
				data->cam_right = cam.right;
				data->cam_up = cam.up;
				data->img_height = (float)tex_size.x;
				data->img_width = (float)tex_size.y;
				data->time = win::timeSinceStart();
				buf->unmap();
				buf->bind(ShaderType::Fragment);
			}
		}

		if (!add_brush.init(nullptr, nullptr, "add_brush")) {
			gfxErrorExit();
		}
		if (!add_brush.shader.addSampler()) {
			gfxErrorExit();
		}
		int brush_ind = add_brush.shader.addBuffer<BrushData>(Usage::Dynamic);

		if (Buffer *buf = add_brush.shader.getBuffer(brush_ind)) {
			if (BrushData *data = buf->map<BrushData>()) {
				data->brush_pos = vec3i(5, 8, -4) * 32;
				data->brush_size = brush_size;
				data->operation = 0;
				buf->unmap();
			}
		}

		if (!edit_sh.init(nullptr, nullptr, "edit_cs")) {
			gfxErrorExit();
		}

		// TODO this never changes, set Usage::Immutable
		int sdfinfo_ind = edit_sh.shader.addBuffer<SDFinfo>(Usage::Dynamic);

		if (Buffer *buf = edit_sh.shader.getBuffer(sdfinfo_ind)) {
			if (SDFinfo *data = buf->map<SDFinfo>()) {
				data->half_tex_size = maintex_size / 2;
				data->padding__0 = 0;
				buf->unmap();
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

		if (Buffer *sdfbuf = main_sh.shader.getBuffer(sdfbuf_ind)) {
			if (SDFData *data = sdfbuf->map<SDFData>()) {
				data->tex_size = maintex_size;
				sdfbuf->unmap();
			}
		}

		GPUClock compute_timer("Compute");

		const auto runCompute = [&]() {
			main_sh.shader.setSRV(ShaderType::Fragment, { nullptr });
			builder.bind();
			TracyD3D11Zone(gfx::tracy_ctx, "Edit");
			compute_timer.start();
			edit_sh.shader.dispatch(threads, { sdfinfo_ind }, {}, { main_texture.uav });
			compute_timer.end();
			builder.unbind();
			//main_sh.shader.dispatch(threads, { sdfbuf_ind }, {}, { main_texture.uav });
			//add_brush.shader.dispatch(threads, { brush_ind }, { brush.srv }, { main_texture.uav });
			main_sh.shader.setSRV(ShaderType::Fragment, { main_texture.srv });
		};

		CPUClock timer("builder");
		
		constexpr int len = 2;
		for (int z = 0; z < len; ++z) {
			for (int y = 0; y < len; ++y) {
				for (int x = 0; x < len; ++x) {
					constexpr vec3 tsize = maintex_size;
					vec3 centre = -(tsize / 2.f) + vec3((float)x, (float)y, (float)z) * (tsize / len);
					vec3 size = (tsize / len) * 0.1f;
					static bool once = true;
					if (once) { once = false; info("size: (%.1f %.1f %.1f) tsize: (%.1f %.1f %.1f)", size.x, size.y, size.z, tsize.x, tsize.y, tsize.z); }
					builder.addBox(centre, size);
				}
			}
		}

		timer.print();

		// builder.addSphere(vec3(-2.5, 0, 0) * 32.f, 5 * 32.f);
		// builder.addBox(vec3(+2.5, 0, 0) * 32.f, vec3(2, 3, 4) * 32.f);
		// builder.addBox(vec3(-1, -2, -3) * 32.f, vec3(2, 3, 4) * 32.f, Operations::None, Alteration::None);

		runCompute();

		while (win::isOpen()) {
			main_sh.poll();
			add_brush.poll();
			edit_sh.poll();

			if (compute_timer.isReady()) {
				compute_timer.print();
			}

			if (isKeyPressed(KEY_ESCAPE)) {
				win::close();
			}

			cam.input();

			//vec3 velocity = {
			//	(float)(isKeyDown(KEY_A) - isKeyDown(KEY_D)),
			//	0.f,
			//	(float)(isKeyDown(KEY_W) - isKeyDown(KEY_S))
			//};

			//float speed = 5.f * win::dt;
			//cam.pos += velocity * speed;

			if (Buffer *buf = main_sh.shader.getBuffer(buf_ind)) {
				if (PSShaderData *data = buf->map<PSShaderData>()) {
					data->cam_pos = cam.pos;
					data->cam_fwd = cam.fwd;
					data->cam_right = cam.right;
					data->cam_up = cam.up;
					data->img_height = (float)tex_size.x;
					data->img_width = (float)tex_size.y;
					data->time = win::timeSinceStart();
					buf->unmap();
					buf->bind(ShaderType::Fragment);
					is_dirty = true;
				}
			}

			if (main_sh.hasUpdated()) {
				is_dirty = true;
				if (main_sh.getChanged() & ShaderType::Compute) {
					runCompute();
					//main_sh.shader.setSRV(ShaderType::Fragment, { nullptr });

					//PerformanceClock clock = "compute shader";
					//main_sh.shader.dispatch(threads, { sdfbuf_ind }, {}, { main_texture.uav });
					//clock.print();

					//main_sh.shader.setSRV(ShaderType::Fragment, { main_texture.srv });
				}
			}
			
			if (add_brush.hasUpdated() || edit_sh.hasUpdated()) {
				is_dirty = true;
				runCompute();
			}


			gfx::begin(Colour::dark_grey);

			if (!Options::get().lazy_render || is_dirty) {
				if (Options::get().lazy_render) {
					info("(Lazy Rendering) rendering scene again");
				}
				gfx::main_rtv.clear(Colour::dark_grey);
				gfx::main_rtv.bind();
				main_sh.shader.bind();
				m.render();
				main_sh.shader.unbind();
			}

			gfx::imgui_rtv.bind();
			fpsWidget();
			mainTargetWidget();
			//ImGui::ShowDemoWindow();
			drawLogger();
			gfx::end();

			if (isKeyPressed(KEY_P)) {
				gfx::main_rtv.takeScreenshot();
			}

			gfx::logD3D11messages();

			is_dirty = false;
		}
	}

	win::cleanup();
}
#endif

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
	
	return dev->CreateBuffer(&desc, nullptr, out);
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