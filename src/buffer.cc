#include "gfx.h"

#include <d3d11.h>

#include "system.h"
#include "utils.h"
#include "macros.h"
#include "tracelog.h"

static D3D11_USAGE usage_to_d3d11[(size_t)Buffer::Usage::Count] = {
    D3D11_USAGE_DEFAULT,
    D3D11_USAGE_IMMUTABLE,
    D3D11_USAGE_DYNAMIC,
    D3D11_USAGE_STAGING
};

Buffer::Buffer(Buffer &&buf) {
    *this = mem::move(buf);
}

Buffer::~Buffer() {
    cleanup();
}

Buffer &Buffer::operator=(Buffer &&buf) {
    if (this != &buf) {
        mem::swap(buffer, buf.buffer);
    }

    return *this;
}

bool Buffer::create(size_t type_size, Usage usage, bool can_write, bool can_read) {
    D3D11_BUFFER_DESC bd;
    mem::zero(bd);
    bd.Usage = usage_to_d3d11[(size_t)usage];
    bd.StructureByteStride = (UINT)type_size;
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

bool Buffer::create(size_t type_size, Usage usage, const void *initial_data, size_t data_count, bool can_write, bool can_read) {
    D3D11_BUFFER_DESC bd;
    mem::zero(bd);
    bd.Usage = usage_to_d3d11[(size_t)usage];
    bd.StructureByteStride = (UINT)type_size;
    bd.ByteWidth = (UINT)(type_size * data_count);
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (usage != Usage::Immutable) {
        if (can_write) {
            bd.CPUAccessFlags |= D3D11_CPU_ACCESS_WRITE;
        }
        if (can_read) {
            bd.CPUAccessFlags |= D3D11_CPU_ACCESS_READ;
        }
    }

    if (initial_data) {
        D3D11_SUBRESOURCE_DATA dd;
        mem::zero(dd);
        dd.pSysMem = initial_data;
        HRESULT hr = gfx::device->CreateBuffer(&bd, &dd, &buffer);
        return SUCCEEDED(hr);
    }

    HRESULT hr = gfx::device->CreateBuffer(&bd, nullptr, &buffer);
    return SUCCEEDED(hr);
}

bool Buffer::createStructured(size_t type_size, size_t count, const void* initial_data) {
    D3D11_BUFFER_DESC desc;
    mem::zero(desc);
    desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    desc.ByteWidth = (UINT)(type_size * count);
    desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    desc.StructureByteStride = (UINT)type_size;
    
    HRESULT hr = E_FAIL;

    if (initial_data) {
        D3D11_SUBRESOURCE_DATA init_data;
        mem::zero(init_data);
        init_data.pSysMem = initial_data;
        hr = gfx::device->CreateBuffer(&desc, &init_data, &buffer);
    }
    else {
        hr = gfx::device->CreateBuffer(&desc, nullptr, &buffer);
    }

    if (FAILED(hr)) {
        err("couldn't create structured buffer");
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
    mem::zero(srv_desc);
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
    srv_desc.Format = DXGI_FORMAT_UNKNOWN;
    srv_desc.BufferEx.NumElements = (UINT)count;

    hr = gfx::device->CreateShaderResourceView(buffer, &srv_desc, &srv);

    if (FAILED(hr)) {
        cleanup();
        err("couldn't create structured buffer's SRV");
        return false;
    }

    D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
    mem::zero(uav_desc);
    uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uav_desc.Format = DXGI_FORMAT_UNKNOWN;
    uav_desc.Buffer.NumElements = (UINT)count;

    hr = gfx::device->CreateUnorderedAccessView(buffer, &uav_desc, &uav);

    if (FAILED(hr)) {
        cleanup();
        err("couldn't create structured buffer's UAV");
        return false;
    }

    return true;
}

void Buffer::cleanup() {
    buffer.destroy();
    srv.destroy();
    uav.destroy();
}

void Buffer::unmap(unsigned int subresource) {
    gfx::context->Unmap(buffer, subresource);
}

void Buffer::bindCBuffer(Buffer &buf, ShaderType type, unsigned int slot) {
    switch (type) {
        case ShaderType::Vertex:   gfx::context->VSSetConstantBuffers(slot, 1, &buf.buffer); break;
        case ShaderType::Fragment: gfx::context->PSSetConstantBuffers(slot, 1, &buf.buffer); break;
        case ShaderType::Compute:  gfx::context->CSSetConstantBuffers(slot, 1, &buf.buffer); break;
    }
}

void Buffer::bindSRV(Buffer &buf, ShaderType type, unsigned int slot) {
    switch (type) {
        case ShaderType::Vertex:   gfx::context->VSSetShaderResources(slot, 1, &buf.srv); break;
        case ShaderType::Fragment: gfx::context->PSSetShaderResources(slot, 1, &buf.srv); break;
        case ShaderType::Compute:  gfx::context->CSSetShaderResources(slot, 1, &buf.srv); break;
    }
}

void Buffer::bindUAV(Buffer &buf, unsigned int slot) {
    gfx::context->CSSetUnorderedAccessViews(slot, 1, &buf.uav, nullptr);
}

void Buffer::unbindCBuffer(ShaderType type, unsigned int slot, size_t count) {
    static ID3D11Buffer* null_bufs[10] = {};
    assert(count < ARRLEN(null_bufs));

    switch (type) {
        case ShaderType::Vertex:   gfx::context->VSSetConstantBuffers(slot, (UINT)count, null_bufs); break;
        case ShaderType::Fragment: gfx::context->PSSetConstantBuffers(slot, (UINT)count, null_bufs); break;
        case ShaderType::Compute:  gfx::context->CSSetConstantBuffers(slot, (UINT)count, null_bufs); break;
    }
}

void Buffer::unbindSRV(ShaderType type, unsigned int slot, size_t count) {
    static ID3D11ShaderResourceView* null_srvs[10] = {};
    assert(count < ARRLEN(null_srvs));

    switch (type) {
        case ShaderType::Vertex:   gfx::context->VSSetShaderResources(slot, (UINT)count, null_srvs); break;
        case ShaderType::Fragment: gfx::context->PSSetShaderResources(slot, (UINT)count, null_srvs); break;
        case ShaderType::Compute:  gfx::context->CSSetShaderResources(slot, (UINT)count, null_srvs); break;
    }
}

void Buffer::unbindUAV(unsigned int slot, size_t count) {
    static ID3D11UnorderedAccessView *null_uavs[10] = {};
    assert(count < ARRLEN(null_uavs));

    gfx::context->CSSetUnorderedAccessViews(slot, (UINT)count, null_uavs, nullptr);
}

#if 0
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

void Buffer::unmapCS(unsigned int subresource, unsigned int slot){
    gfx::context->Unmap(buffer, subresource);
    gfx::context->CSSetConstantBuffers(slot, 1, &buffer);
}
#endif

void* Buffer::map(unsigned int subresource) {
    D3D11_MAPPED_SUBRESOURCE resource;
    HRESULT hr = gfx::context->Map(buffer, subresource, D3D11_MAP_WRITE_DISCARD, 0, &resource);
    if (FAILED(hr)) {
        err("couldn't map buffer");
        return nullptr;
    }
    return resource.pData;
}