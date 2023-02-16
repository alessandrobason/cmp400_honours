#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "vec.h"
#include "colours.h"
#include "d3d11_fwd.h"
#include "texture.h"

namespace gfx {
	extern dxptr<ID3D11Device> device;
	extern dxptr<ID3D11DeviceContext> context;
	extern dxptr<IDXGISwapChain> swapchain;
	extern RenderTexture imgui_rtv;
	extern RenderTexture main_rtv;

	void init();
	void cleanup();

	void begin(Colour clear_colour);
	void end();

	bool createDevice();
	void cleanupDevice();
	void createImGuiRTV();
	void cleanupImGuiRTV();

#ifndef NDEBUG
	void logD3D11messages();
#endif
} // namespace gfx

namespace win {
	extern HWND hwnd;
	extern HINSTANCE hinstance;
	extern const TCHAR *windows_class_name;
	extern float dt;
	extern float fps;

	bool isOpen();
	void poll();
	void create(const char *name, int width, int height);
	void close();
	void cleanup();
	float timeSinceStart();

	vec2i getSize();
} //namespace win
