#pragma once

#include "common.h"
#include "texture.h"

namespace str { struct tstr; }

namespace gfx {
	extern dxptr<ID3D11Device> device;
	extern dxptr<ID3D11DeviceContext> context;
	extern dxptr<IDXGISwapChain> swapchain;
	extern RenderTexture imgui_rtv;
	extern RenderTexture main_rtv;
	extern void *tracy_ctx;

	void init();
	void cleanup();

	void begin();
	void end();

	bool createDevice();
	void cleanupDevice();
	void createImGuiRTV();
	void cleanupImGuiRTV();

	const vec4 &getMainRTVBounds();
	void setMainRTVBounds(const vec4 &bounds);

	bool isMainRTVActive();
	void setMainRTVActive(bool is_active);

	void captureFrame();

	void logD3D11messages();
} // namespace gfx

namespace win {
	extern win32_handle_t hwnd;
	extern win32_handle_t hinstance;
	extern float dt;
	extern float fps;

	bool isOpen();
	void poll();
	void create(const char *name, int width, int height);
	void close();
	void cleanup();

	const str::tstr &getBaseWindowName();
	void setWindowName(const str::tstr &name);

	float timeSinceStart();

	vec2i getSize();
} //namespace win
