#pragma once

struct ID3D11Device;
struct ID3D11Debug;
struct ID3D11DeviceContext;
struct IDXGISwapChain;
struct ID3D11Texture2D;
struct ID3D11Texture3D;
struct ID3D11RenderTargetView;
struct ID3D11ShaderResourceView;
struct ID3D11DepthStencilView;
struct ID3D11Buffer;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11ComputeShader;
struct ID3D11InputLayout;

// win32 forward stuff

#ifdef UNICODE
typedef wchar_t TCHAR;
#else
typedef char TCHAR;
#endif

typedef void *win32_handle_t;

typedef struct {
	unsigned long long Internal;
	unsigned long long InternalHigh;
	union {
		struct {
			unsigned long Offset;
			unsigned long OffsetHigh;
		} unused__struct;
		void *Pointer;
	} unused__union;

	win32_handle_t hEvent;
} win32_overlapped_t;