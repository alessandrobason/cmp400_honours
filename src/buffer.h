#pragma once

#include "d3d11_fwd.h"

enum class Usage {
	Default,
	Immutable,
	Dynamic,
	Staging,
	Count,
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
	void cleanup();

	template<typename T>
	T* map(unsigned int subresource = 0) {
		return (T*)map(subresource);
	}

	void* map(unsigned int subresource = 0);
	void unmapVS(unsigned int subresource = 0, unsigned int slot = 0);
	void unmapPS(unsigned int subresource = 0, unsigned int slot = 0);
	void unmapGS(unsigned int subresource = 0, unsigned int slot = 0);

	dxptr<ID3D11Buffer> buffer = nullptr;
};
