#include "system.h"

#include <d3d11.h>
#include <windowsx.h> // GET_X_LPARAM(), GET_Y_LPARAM()
#include <assert.h>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>
#include <sokol_time.h>

#include "tracelog.h"
#include "str_utils.h"
#include "options.h"
#include "maths.h"
#include "keys.h"
#include "macros.h"

static LRESULT wndProc(HWND, UINT, WPARAM, LPARAM);

Options g_options;

namespace gfx {
	ID3D11Device *device = nullptr;
	ID3D11DeviceContext *context = nullptr;
	ID3D11Debug *debugdev = nullptr;
	IDXGISwapChain *swapchain = nullptr;
	//ID3D11RenderTargetView *imgui_rtv = nullptr;
	RenderTexture imgui_rtv;
	RenderTexture main_rtv;

	void init() {
		if (!createDevice()) {
			fatal("couldn't create d3d11 device");
		}

		// -- Initialize ImGui --
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO &io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	
		// Setup Dear ImGui style
		ImGui::StyleColorsDark();

		// Setup Platform/Renderer backends
		ImGui_ImplWin32_Init(win::hwnd);
		ImGui_ImplDX11_Init(device, context);

		bool rtv_success = main_rtv.create(1920, 1080);
		if (!rtv_success) {
			fatal("couldn't create main RTV");
		}
	}

	void cleanup() {
		// Cleanup ImGui
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();

		main_rtv.cleanup();
		cleanupDevice();
	}

	void begin(Colour clear_colour) {
		UNUSED(clear_colour);

		main_rtv.bind();
		main_rtv.clear(math::lerp(col::coral, col::indigo, sinf(win::timeSinceStart())));

		imgui_rtv.bind();
		//context->OMSetRenderTargets(1, &imgui_rtv, nullptr);
		//context->ClearRenderTargetView(imgui_rtv, clear_colour.data);

		// Start the Dear ImGui frame
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		ImGui::DockSpaceOverViewport();
	}

	void end() {
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		
		swapchain->Present(g_options.vsync, 0);
	}

	bool createDevice() {
		DXGI_SWAP_CHAIN_DESC sd;
		str::memzero(sd);
		sd.BufferCount = 2;
		sd.BufferDesc.Width = 0;
		sd.BufferDesc.Height = 0;
		sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.BufferDesc.RefreshRate.Numerator = 60;
		sd.BufferDesc.RefreshRate.Denominator = 1;
		sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.OutputWindow = win::hwnd;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.Windowed = TRUE;
		sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

		UINT createDeviceFlags = 0;
#ifndef NDEBUG
		createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
		D3D_FEATURE_LEVEL featureLevel;
		const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
		if (D3D11CreateDeviceAndSwapChain(
			NULL,
			D3D_DRIVER_TYPE_HARDWARE,
			NULL,
			createDeviceFlags,
			featureLevelArray,
			2,
			D3D11_SDK_VERSION,
			&sd,
			&swapchain,
			&device,
			&featureLevel,
			&context
		) != S_OK) {
			return false;
		}

#ifndef NDEBUG
		HRESULT hr = device->QueryInterface(__uuidof(ID3D11Debug), (void **)&debugdev);
		assert(SUCCEEDED(hr));
#endif

		createImGuiRTV();

		return true;
	}
	
	void cleanupDevice() {
		cleanupImGuiRTV();
		SAFE_RELEASE(swapchain);
		SAFE_RELEASE(context);
		SAFE_RELEASE(device);
#ifndef NDEBUG
		debugdev->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL);
		SAFE_RELEASE(debugdev);
#endif
	}

	void createImGuiRTV() {
		imgui_rtv.createFromBackbuffer();
	}

	void cleanupImGuiRTV() {
		imgui_rtv.cleanup();
		//SAFE_RELEASE(imgui_rtv);
	}
} // namespace gfx

namespace win {
	HWND hwnd = nullptr;
	HINSTANCE hinstance = nullptr;
	const TCHAR *windows_class_name = TEXT("thesis-app");
	float dt = 1.f / 60.f;
	float fps = 60.f;
	static uint64_t laptime = 0;
	static bool is_open = true;
	static vec2i size;

	bool isOpen() {
		return is_open;
	}

	void poll() {
		MSG msg;
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			if (msg.message == WM_QUIT) {
				win::close();
			}
		}

		dt = (float)stm_sec(stm_laptime(&laptime));
		fps = (fps + 1.f / dt) / 2.f;
	}

	void close() {
		is_open = false;
	}

	void create(const char *name, int width, int height) {
		size = { width, height };
		
		hinstance = GetModuleHandle(NULL);

		WNDCLASSEX wc{};
		wc.cbSize = sizeof(wc);
		wc.style = CS_CLASSDC;
		wc.lpfnWndProc = wndProc;
		wc.hInstance = hinstance;
		wc.lpszClassName = windows_class_name;
		RegisterClassEx(&wc);

#if UNICODE
		constexpr int max_len = 255;
		wchar_t window_name[max_len] = { 0 };
		bool converted = str::ansiToWide(name, strlen(name), window_name, max_len);
		if (!converted) {
			fatal("couldn't convert window name (%s) to wchar", window_name);
		}
#else
		const char *window_name = name;
#endif

		hwnd = CreateWindow(
			windows_class_name,
			window_name,
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT,
			width, height,
			NULL, NULL,
			hinstance,
			NULL
		);

		gfx::init();

		ShowWindow(hwnd, SW_SHOWDEFAULT);
		UpdateWindow(hwnd);

		g_options.load();

		stm_setup();
		laptime = stm_now();
	}

	void cleanup() {
		gfx::cleanup();

		DestroyWindow(hwnd);
		UnregisterClass(windows_class_name, hinstance);
		hwnd = nullptr;
		hinstance = nullptr;
	}

	float timeSinceStart() {
		return (float)stm_sec(stm_since(0));
	}

	vec2i getSize() {
		return size;
	}
} // namespace sys

static bool keys_state[KEY__COUNT] = {0};
static bool prev_keys_state[KEY__COUNT] = {0};
static uint32_t mouse_buttons_down = 0;
static vec2i mouse_position;
static vec2i mouse_relative;
static float mouse_wheel = 0.f;

bool isKeyDown(Keys key) {
	return keys_state[key];
}

bool isKeyUp(Keys key) {
	return !keys_state[key];
}

bool isKeyPressed(Keys key) {
	bool pressed = !prev_keys_state[key] && keys_state[key];
	prev_keys_state[key] = keys_state[key];
	return pressed;
}

bool isMouseDown(Mouse mouse) {
	return mouse_buttons_down & (1 << (uint32_t)mouse);
}

bool isMouseUp(Mouse mouse) {
	return !isMouseDown(mouse);
}

vec2i getMousePos() {
	return mouse_position;
}

vec2 getMousePosNorm() {
	return (vec2)mouse_position / (vec2)win::size;
}

vec2i getMousePosRel() {
	return mouse_relative;
}

float getMouseWheel() {
	return mouse_wheel;
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#include "tracelog.h"

LRESULT wndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam))
		return true;
	
	switch (msg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	case WM_SIZE:
		if (gfx::device && wparam != SIZE_MINIMIZED) {
			win::size = { LOWORD(lparam), HIWORD(lparam) };
			
			gfx::cleanupImGuiRTV();
			gfx::swapchain->ResizeBuffers(
				0, 
				(UINT)win::size.x,
				(UINT)win::size.y,
				DXGI_FORMAT_UNKNOWN,
				0
			);
			gfx::createImGuiRTV();
		}
		return 0;

	case WM_SYSCOMMAND:
		if ((wparam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
		break;
	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	{
		bool is_key_down = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
		uintptr_t vk = wparam;
		if ((wparam == VK_RETURN) && (HIWORD(lparam) & KF_EXTENDED))
			vk = VK_RETURN + KF_EXTENDED;
		
		Keys key = win32ToKeys(vk);
		prev_keys_state[key] = keys_state[key];
		keys_state[key] = is_key_down;

		return 0;
	}

	case WM_MOUSEMOVE:
	{
		vec2i old_pos = mouse_position;
		mouse_position = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
		mouse_relative = mouse_position - old_pos;
		return 0;
	}

	case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK:
	case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK:
	{
		Mouse btn = win32ToMouse(msg);
		mouse_buttons_down |= 1 << (uint32_t)btn;
		return 0;
	}
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	{
		Mouse btn = win32ToMouse(msg);
		mouse_buttons_down &= ~(1 << (uint32_t)btn);
		return 0;
	}
	case WM_MOUSEWHEEL:
		mouse_wheel = (float)GET_WHEEL_DELTA_WPARAM(wparam) / (float)WHEEL_DELTA;
		return 0;
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);
}
