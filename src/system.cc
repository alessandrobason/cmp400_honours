#include "system.h"

#include <d3d11.h>
#include <windowsx.h> // GET_X_LPARAM(), GET_Y_LPARAM()
#include <assert.h>
#include <float.h>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>
#include <sokol_time.h>

#include "tracelog.h"
#include "utils.h"
#include "options.h"
#include "maths.h"
#include "input.h"
#include "macros.h"

static LRESULT wndProc(HWND, UINT, WPARAM, LPARAM);

Options g_options;

namespace gfx {
	ID3D11Device *device = nullptr;
	ID3D11DeviceContext *context = nullptr;
	ID3D11Debug *debugdev = nullptr;
	ID3D11InfoQueue *infodev = nullptr;
	IDXGISwapChain *swapchain = nullptr;
	ID3D11DepthStencilState* depth_stencil_state = nullptr;
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

		//bool rtv_success = main_rtv.create(1920, 1080);
		bool rtv_success = main_rtv.create(1000, 1000);
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

		//main_rtv.bind();
		//main_rtv.clear(clear_colour);

		//imgui_rtv.bind();
		imgui_rtv.clear(Colour::black);

		// Start the Dear ImGui frame
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		ImGui::DockSpaceOverViewport();
	}

	void end() {
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		
		gfx::context->OMSetRenderTargets(0, nullptr, nullptr);
		swapchain->Present(g_options.vsync, 0);
	}

	bool createDevice() {
		DXGI_SWAP_CHAIN_DESC sd;
		mem::zero(sd);
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
		HRESULT hr = D3D11CreateDeviceAndSwapChain(
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
		);
		if (FAILED(hr)) {
			err("couldn't create device");
			return false;
		}

#ifndef NDEBUG
		hr = device->QueryInterface(__uuidof(ID3D11Debug), (void **)&debugdev);
		assert(SUCCEEDED(hr));
		hr = device->QueryInterface(__uuidof(ID3D11InfoQueue), (void **)&infodev);
		assert(SUCCEEDED(hr));

		// D3D11_MESSAGE_CATEGORY categories[] = {
		// 	D3D11_MESSAGE_CATEGORY_STATE_CREATION,
		// };
		// D3D11_INFO_QUEUE_FILTER filter;
		// mem::zero(filter);
		// filter.DenyList.NumCategories = ARRLEN(categories);
		// filter.DenyList.pCategoryList = categories;
		// infodev->PushStorageFilter(&filter);
#endif

		D3D11_DEPTH_STENCIL_DESC dd;
		mem::zero(dd);
		dd.DepthFunc = D3D11_COMPARISON_ALWAYS;
		dd.DepthEnable = false;
		dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		dd.DepthFunc = D3D11_COMPARISON_ALWAYS;
		dd.StencilEnable = false;
		dd.FrontFace.StencilFailOp = dd.FrontFace.StencilDepthFailOp = dd.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		dd.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		dd.BackFace = dd.FrontFace;
		hr = device->CreateDepthStencilState(&dd, &depth_stencil_state);
		if (FAILED(hr)) {
			err("couldn't create depth stencil state");
			return false;
		}
		context->OMSetDepthStencilState(depth_stencil_state, 0);

		createImGuiRTV();

		return true;
	}
	
	void cleanupDevice() {
#ifndef NDEBUG
		// we need this as it doesn't report memory leak otherwise
		infodev->PushEmptyStorageFilter();
#endif
		cleanupImGuiRTV();
		SAFE_RELEASE(depth_stencil_state);
		SAFE_RELEASE(swapchain);
		SAFE_RELEASE(context);
		SAFE_RELEASE(device);
#ifndef NDEBUG
		SAFE_RELEASE(infodev);
		debugdev->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL);
		SAFE_RELEASE(debugdev);
#endif
	}

	void createImGuiRTV() {
		imgui_rtv.createFromBackbuffer();
	}

	void cleanupImGuiRTV() {
		imgui_rtv.cleanup();
	}

#ifndef NDEBUG
	void gfx::logD3D11messages() {
		UINT64 message_count = infodev->GetNumStoredMessages();

		D3D11_MESSAGE* msg = nullptr;
		SIZE_T old_size = 0;
	
		for (UINT64 i = 0; i < message_count; ++i) {
			SIZE_T msg_size = 0;
			infodev->GetMessage(i, nullptr, &msg_size);
			
			if (msg_size > old_size) {
				D3D11_MESSAGE *new_msg = (D3D11_MESSAGE *)realloc(msg, msg_size);
				if (!new_msg) {
					fatal("couldn't reallocate message");
				}
				msg = new_msg;
			}
			infodev->GetMessage(i, msg, &msg_size);
			assert(msg);
			
			switch (msg->Severity) {
			case D3D11_MESSAGE_SEVERITY_CORRUPTION: fatal("%.*s", msg->DescriptionByteLength, msg->pDescription); break;
			case D3D11_MESSAGE_SEVERITY_ERROR:      err("%.*s", msg->DescriptionByteLength, msg->pDescription); break;
			case D3D11_MESSAGE_SEVERITY_WARNING:    warn("%.*s", msg->DescriptionByteLength, msg->pDescription); break;
			case D3D11_MESSAGE_SEVERITY_INFO:       info("%.*s", msg->DescriptionByteLength, msg->pDescription); break;
			case D3D11_MESSAGE_SEVERITY_MESSAGE:    tall("%.*s", msg->DescriptionByteLength, msg->pDescription); break;
			}
		}

		free(msg);
		infodev->ClearStoredMessages();
	}
#endif
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
		poll();
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
		if (dt) {
			fps = (fps + 1.f / dt) / 2.f;
		}
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

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

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
		
		setKeyState(win32ToKeys(vk), is_key_down);

		return 0;
	}

	case WM_MOUSEMOVE:
	{
		setMousePosition({ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) });
		return 0;
	}

	case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK:
	case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK:
	{
		setMouseButtonState(win32ToMouse(msg), true);
		return 0;
	}
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	{
		setMouseButtonState(win32ToMouse(msg), false);
		return 0;
	}
	case WM_MOUSEWHEEL:
		setMouseWheel((float)GET_WHEEL_DELTA_WPARAM(wparam) / (float)WHEEL_DELTA);
		return 0;
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);
}
