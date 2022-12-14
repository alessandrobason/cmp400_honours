#include "buffer.h"

#include <d3d11.h>

#include "system.h"
#include "utils.h"
#include "macros.h"
#include "tracelog.h"

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
    mem::zero(bd);
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

void Buffer::unmapVS(unsigned int subresource, unsigned int slot) {
    gfx::context->Unmap(buffer, subresource);
    gfx::context->VSSetConstantBuffers(slot, 1, &buffer);
}

void Buffer::unmapPS(unsigned int subresource, unsigned int slot) {
    gfx::context->Unmap(buffer, subresource);
    gfx::context->PSSetConstantBuffers(slot, 1, &buffer);
}

void Buffer::unmapGS(unsigned int subresource, unsigned int slot) {
    gfx::context->Unmap(buffer, subresource);
    gfx::context->GSSetConstantBuffers(slot, 1, &buffer);
}

void* Buffer::map(unsigned int subresource) {
    D3D11_MAPPED_SUBRESOURCE resource;
    HRESULT hr = gfx::context->Map(buffer, subresource, D3D11_MAP_WRITE_DISCARD, 0, &resource);
    if (FAILED(hr)) {
        err("couldn't map buffer");
        return nullptr;
    }
    return resource.pData;
}