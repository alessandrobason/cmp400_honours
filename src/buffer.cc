#include "buffer.h"

#include <d3d11.h>

#include "system.h"
#include "str_utils.h"
#include "macros.h"

static D3D11_USAGE usage_to_d3d11[(size_t)Usage::Count] = {
    D3D11_USAGE_DEFAULT,
    D3D11_USAGE_IMMUTABLE,
    D3D11_USAGE_DYNAMIC,
    D3D11_USAGE_STAGING
};

Buffer::Buffer(Buffer &&buf) {
    *this = std::move(buf);
}

Buffer::~Buffer() {
    cleanup();
}

Buffer &Buffer::operator=(Buffer &&buf) {
    if (this != &buf) {
        buffer = buf.buffer;
        buf.buffer = nullptr;
    }

    return *this;
}

bool Buffer::create(size_t type_size, Usage usage, bool can_write, bool can_read) {
    D3D11_BUFFER_DESC bd;
    str::memzero(bd);
    bd.Usage = usage_to_d3d11[(size_t)usage];
    bd.ByteWidth = (UINT)type_size;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (can_write) {
        bd.CPUAccessFlags |= D3D11_CPU_ACCESS_WRITE;
    }
    if (can_read) {
        bd.CPUAccessFlags |= D3D11_CPU_ACCESS_READ;
    }
    
    HRESULT hr = gfx::device->CreateBuffer(&bd, nullptr, &buffer);
    return SUCCEEDED(hr);
}

void Buffer::cleanup() {
    SAFE_RELEASE(buffer);
}
