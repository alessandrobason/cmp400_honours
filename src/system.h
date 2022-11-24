#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "keys.h"
#include "vec.h"
#include "colours.h"
#include "d3d11_fwd.h"
#include "texture.h"

namespace gfx {
	extern ID3D11Device *device;
	extern ID3D11DeviceContext *context;
	extern IDXGISwapChain *swapchain;
	//extern ID3D11RenderTargetView *imgui_rtv;
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

bool isKeyDown(Keys key);
bool isKeyUp(Keys key);
bool isKeyPressed(Keys key);
bool isMouseDown(Mouse mouse);
bool isMouseUp(Mouse mouse);
vec2i getMousePos();
vec2 getMousePosNorm();
vec2i getMousePosRel();
float getMouseWheel();
