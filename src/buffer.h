#pragma once

#include <stdint.h>
#include "d3d11_fwd.h"

enum class Usage {
	Default,   // can read/write from GPU
	Immutable, // can be read from the GPU
	Dynamic,   // can be read from the GPU and written by the CPU
	Staging,   // used to transfer from the GPU to the CPU
	Count,
};

enum class ShaderType : uint8_t {
	None = 0,
	Vertex = 1 << 0,
	Fragment = 1 << 1,
	Compute = 1 << 2,
};

struct Buffer {
	Buffer() = default;
	Buffer(const Buffer &buf) = delete;
	Buffer(Buffer &&buf);
	~Buffer();

	Buffer &operator=(Buffer &&buf);

	template<typename T>
	bool create(Usage usage, bool can_write = true, bool can_read = false) {
		return create(sizeof(T), usage, can_write, can_read);
	}

	bool create(size_t type_size, Usage usage, bool can_write = true, bool can_read = false);
	bool create(size_t type_size, Usage usage, const void *initial_data, size_t data_count = 1, bool can_write = true, bool can_read = false);
	void cleanup();

	template<typename T>
	T* map(unsigned int subresource = 0) {
		return (T*)map(subresource);
	}

	void* map(unsigned int subresource = 0);
	void unmap(unsigned int subresource = 0);
	void bind(ShaderType type, unsigned int slot = 0);
	void unbind(ShaderType type, unsigned int slot = 0);

	dxptr<ID3D11Buffer> buffer = nullptr;
};
