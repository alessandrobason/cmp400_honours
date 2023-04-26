#pragma once

#include "gfx_common.h"
#include "handle.h"

enum class Bind : uint8_t {
	None = 0,
	CpuRead      = 1u << 0,
	CpuWrite     = 1u << 1,
	CpuReadWrite = CpuRead | CpuWrite,
	GpuRead      = 1u << 2,
	GpuWrite     = 1u << 3,
	GpuReadWrite = GpuRead | GpuWrite,
	All          = CpuReadWrite | GpuReadWrite,
};

inline Bind operator|(Bind left, Bind right) { return (Bind)((uint8_t)left | (uint8_t)right); }

struct Buffer {
	enum class Usage {
		Default,   // can read/write from GPU
		Immutable, // can be read from the GPU
		Dynamic,   // can be read from the GPU and written by the CPU
		Staging,   // used to transfer from the GPU to the CPU
		Count,
	};

	Buffer(const Buffer &buf) = delete;
	Buffer(Buffer &&buf);
	~Buffer();
	Buffer &operator=(Buffer &&buf);

	// -- handle stuff --
	static Handle<Buffer> make();
	static Handle<Buffer> makeConstant(size_t type_size, Usage usage, bool cpu_can_write = true, bool cpu_can_read = false, const void *initial_data = nullptr, size_t data_count = 1);
	static Handle<Buffer> makeStructured(size_t type_size, size_t count = 1, Bind bind = Bind::GpuReadWrite, const void *initial_data = nullptr);
	static void cleanAll();

	template<typename T>
	static Handle<Buffer> makeConstant(Usage usage, bool can_write = true, bool can_read = false, const void *initial_data = nullptr, size_t data_count = 1) {
		return makeConstant(sizeof(T), usage, can_write, can_read, initial_data, data_count);
	}

	template<typename T>
	static Handle<Buffer> makeStructured(size_t count = 1, Bind bind = Bind::GpuReadWrite, const void *initial_data = nullptr) {
		return makeStructured(sizeof(T), count, bind, initial_data);
	}
	// ------------------

	void cleanup();

	void resize(size_t new_count);

	template<typename T>
	T *map(unsigned int subresource = 0) {
		return (T *)map(subresource);
	}

	void *map(unsigned int subresource = 0);
	//void *mapRegion();
	void unmap(unsigned int subresource = 0);

	void bindCBuffer(ShaderType type, unsigned int slot = 0) { bindCBuffer(*this, type, slot); }
	void bindSRV(ShaderType type, unsigned int slot = 0) { bindSRV(*this, type, slot); }
	void bindUAV(unsigned int slot = 0) { bindUAV(*this, slot); }

	static void bindCBuffer(Buffer &buf, ShaderType type, unsigned int slot = 0);
	static void bindSRV(Buffer &buf, ShaderType type, unsigned int slot = 0);
	static void bindUAV(Buffer &buf, unsigned int slot = 0);
	static void unbindCBuffer(ShaderType type, unsigned int slot = 0, size_t count = 1);
	static void unbindSRV(ShaderType type, unsigned int slot = 0, size_t count = 1);
	static void unbindUAV(unsigned int slot = 0, size_t count = 1);

	dxptr<ID3D11Buffer> buffer = nullptr;
	dxptr<ID3D11UnorderedAccessView> uav = nullptr;
	dxptr<ID3D11ShaderResourceView> srv = nullptr;

private:
	Buffer() = default;
};