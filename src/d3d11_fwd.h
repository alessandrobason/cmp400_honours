#pragma once

struct IUnknown;
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
struct ID3D11UnorderedAccessView;
struct ID3D11RasterizerState;
struct ID3D11SamplerState;
struct ID3D11Query;
struct ID3D11DeviceChild;

void safeRelease(IUnknown *ptr);

// unique_ptr-like structure for d3d11 data
template<typename T>
struct dxptr {
	dxptr() = default;
	dxptr(T *ptr) : ptr(ptr) {}
	dxptr(dxptr &&other) { swap(other); }
	dxptr &operator=(dxptr &&other) { if (ptr != other.ptr) swap(other); return *this; }
	~dxptr() { destroy(); }

	void swap(dxptr &other) { T *tmp = ptr; ptr = other.ptr; other.ptr = tmp; }
	void destroy() { safeRelease(ptr); ptr = nullptr; }

	T *get() { return ptr; }
	const T *get() const { return ptr; }

	operator bool() const { return ptr != nullptr; }
	T *operator->() { return ptr; }
	const T *operator->() const { return ptr; }

	T **operator&() { return &ptr; }

	operator T *() { return ptr; }
	operator const T *() const { return ptr; }

private:
	T *ptr = nullptr;
};

// win32 forward stuff

#define SAFE_CLOSE(h) if ((h) && (h) != INVALID_HANDLE_VALUE) { CloseHandle(h); (h) = nullptr; }

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

