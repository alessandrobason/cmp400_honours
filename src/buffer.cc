#include "buffer.h"

#include <d3d11.h>

#include "system.h"
#include "utils.h"
#include "tracelog.h"
#include "gfx_factory.h"

static GFXFactory<Buffer> buffer_factory;

uint8_t operator&(Bind left, Bind right) { return (uint8_t)left & (uint8_t)right; }

static bool bufMakeConstant(Buffer *buf, size_t type_size, Buffer::Usage usage, bool cpu_can_write, bool cpu_can_read, const void *initial_data, size_t data_count);
static bool bufMakeStructured(Buffer *buf, size_t type_size, size_t count, Bind bind, const void *initial_data);

static D3D11_USAGE usage_to_d3d11[(size_t)Buffer::Usage::Count] = {
    D3D11_USAGE_DEFAULT,
    D3D11_USAGE_IMMUTABLE,
    D3D11_USAGE_DYNAMIC,
    D3D11_USAGE_STAGING
};

static Bind descToBind(const D3D11_BUFFER_DESC &desc) {
    Bind bind = Bind::None;
    if (desc.BindFlags      & D3D11_BIND_SHADER_RESOURCE)  bind = bind | Bind::GpuRead;
    if (desc.BindFlags      & D3D11_BIND_UNORDERED_ACCESS) bind = bind | Bind::GpuWrite;
    if (desc.CPUAccessFlags & D3D11_CPU_ACCESS_READ)       bind = bind | Bind::CpuRead;
    if (desc.CPUAccessFlags & D3D11_CPU_ACCESS_WRITE)      bind = bind | Bind::CpuWrite;
    return bind;
}

Buffer::Buffer(Buffer &&buf) {
    *this = mem::move(buf);
}

Buffer::~Buffer() {
    cleanup();
}

Buffer &Buffer::operator=(Buffer &&buf) {
    if (this != &buf) {
        mem::swap(buffer, buf.buffer);
        mem::swap(srv, buf.srv);
        mem::swap(uav, buf.uav);
    }

    return *this;
}

Handle<Buffer> Buffer::make() {
    return buffer_factory.getNew();
}

Handle<Buffer> Buffer::makeConstant(
    size_t type_size, 
    Usage usage, 
    bool cpu_can_write,
    bool cpu_can_read,
    const void *initial_data, 
    size_t data_count
) {
    Buffer *newbuf = buffer_factory.getNew();

    if (!bufMakeConstant(newbuf, type_size, usage, cpu_can_write, cpu_can_read, initial_data, data_count)) {
        buffer_factory.popLast();
        return nullptr;
    }

    return newbuf;
}

Handle<Buffer> Buffer::makeStructured(
    size_t type_size, 
    size_t count,
    Bind bind,
    const void *initial_data
) {
    Buffer *newbuf = buffer_factory.getNew();

    if (!bufMakeStructured(newbuf, type_size, count, bind, initial_data)) {
        buffer_factory.popLast();
        return nullptr;
    }

    return newbuf;
}

void Buffer::cleanup() {
    buffer.destroy();
    srv.destroy();
    uav.destroy();
}

void Buffer::resize(size_t new_count) {
    D3D11_BUFFER_DESC desc;
    buffer->GetDesc(&desc);

    // it is a structured buffer
    if (desc.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED) {
        size_t type_size = desc.StructureByteStride;
        size_t old_count = desc.ByteWidth / type_size;
        if (old_count < new_count) {
            Handle<Buffer> newbuf = Buffer::makeStructured(type_size, new_count, descToBind(desc));
            assert(newbuf);
            gfx::context->CopySubresourceRegion(newbuf->buffer, 0, 0, 0, 0, buffer, 0, nullptr);
            *this = mem::move(*newbuf.get());
            newbuf->cleanup();
            buffer_factory.popLast();
        }
    }
    // it is a constant buffer
    else if (desc.BindFlags & D3D11_BIND_CONSTANT_BUFFER) {
        assert(false); // TODO
    }
}

void Buffer::bindCBuffer(Buffer &buf, ShaderType type, uint slot) {
    switch (type) {
        case ShaderType::Vertex:   gfx::context->VSSetConstantBuffers(slot, 1, &buf.buffer); break;
        case ShaderType::Fragment: gfx::context->PSSetConstantBuffers(slot, 1, &buf.buffer); break;
        case ShaderType::Compute:  gfx::context->CSSetConstantBuffers(slot, 1, &buf.buffer); break;
    }
}

void Buffer::bindSRV(Buffer &buf, ShaderType type, uint slot) {
    switch (type) {
        case ShaderType::Vertex:   gfx::context->VSSetShaderResources(slot, 1, &buf.srv); break;
        case ShaderType::Fragment: gfx::context->PSSetShaderResources(slot, 1, &buf.srv); break;
        case ShaderType::Compute:  gfx::context->CSSetShaderResources(slot, 1, &buf.srv); break;
    }
}

void Buffer::bindUAV(Buffer &buf, uint slot) {
    gfx::context->CSSetUnorderedAccessViews(slot, 1, &buf.uav, nullptr);
}

void Buffer::unbindCBuffer(ShaderType type, uint slot, size_t count) {
    static ID3D11Buffer* null_bufs[10] = {};
    assert(count < ARRLEN(null_bufs));

    switch (type) {
        case ShaderType::Vertex:   gfx::context->VSSetConstantBuffers(slot, (UINT)count, null_bufs); break;
        case ShaderType::Fragment: gfx::context->PSSetConstantBuffers(slot, (UINT)count, null_bufs); break;
        case ShaderType::Compute:  gfx::context->CSSetConstantBuffers(slot, (UINT)count, null_bufs); break;
    }
}

void Buffer::unbindSRV(ShaderType type, uint slot, size_t count) {
    static ID3D11ShaderResourceView* null_srvs[10] = {};
    assert(count < ARRLEN(null_srvs));

    switch (type) {
        case ShaderType::Vertex:   gfx::context->VSSetShaderResources(slot, (UINT)count, null_srvs); break;
        case ShaderType::Fragment: gfx::context->PSSetShaderResources(slot, (UINT)count, null_srvs); break;
        case ShaderType::Compute:  gfx::context->CSSetShaderResources(slot, (UINT)count, null_srvs); break;
    }
}

void Buffer::unbindUAV(uint slot, size_t count) {
    static ID3D11UnorderedAccessView *null_uavs[10] = {};
    assert(count < ARRLEN(null_uavs));

    gfx::context->CSSetUnorderedAccessViews(slot, (UINT)count, null_uavs, nullptr);
}

void* Buffer::map(uint subresource) {
    D3D11_MAPPED_SUBRESOURCE resource;
    HRESULT hr = gfx::context->Map(buffer, subresource, D3D11_MAP_WRITE_DISCARD, 0, &resource);
    if (FAILED(hr)) {
        err("couldn't map buffer");
        return nullptr;
    }
    return resource.pData;
}

void Buffer::unmap(uint subresource) {
    gfx::context->Unmap(buffer, subresource);
}

// == PRIVATE FUNCTIONS ==================================================

static bool bufMakeConstant(Buffer *buf, size_t type_size, Buffer::Usage usage, bool cpu_can_write, bool cpu_can_read, const void *initial_data, size_t data_count) {
    assert(buf);
    buf->cleanup();
    
    D3D11_BUFFER_DESC bd;
    mem::zero(bd);
    bd.Usage = usage_to_d3d11[(size_t)usage];
    bd.ByteWidth = (UINT)(type_size * data_count);

    if (usage != Buffer::Usage::Staging) {
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    }

    if (usage != Buffer::Usage::Immutable) {
        if (cpu_can_write) {
            bd.CPUAccessFlags |= D3D11_CPU_ACCESS_WRITE;
        }
        if (cpu_can_read) {
            bd.CPUAccessFlags |= D3D11_CPU_ACCESS_READ;
        }
    }

    HRESULT hr = E_FAIL;

    if (initial_data) {
        D3D11_SUBRESOURCE_DATA dd;
        mem::zero(dd);
        dd.pSysMem = initial_data;
        hr = gfx::device->CreateBuffer(&bd, &dd, &buf->buffer);
    }
    else {
        hr = gfx::device->CreateBuffer(&bd, nullptr, &buf->buffer);
    }

    if (FAILED(hr)) {
        err("couldn't create buffer");
        return false;
    }

    return true;
}

static bool bufMakeStructured(Buffer *buf, size_t type_size, size_t count, Bind bind, const void *initial_data) {
    assert(buf);
    buf->cleanup();

    D3D11_BUFFER_DESC desc;
    mem::zero(desc);
    if (bind & Bind::GpuRead)  desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
    if (bind & Bind::GpuWrite) desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
    if (bind & Bind::CpuRead)  desc.CPUAccessFlags |= D3D11_CPU_ACCESS_READ;
    if (bind & Bind::CpuWrite) {
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.CPUAccessFlags |= D3D11_CPU_ACCESS_WRITE;
    }
    desc.ByteWidth = (UINT)(type_size * count);
    desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    desc.StructureByteStride = (UINT)type_size;

    HRESULT hr = E_FAIL;

    if (initial_data) {
        D3D11_SUBRESOURCE_DATA init_data;
        mem::zero(init_data);
        init_data.pSysMem = initial_data;
        hr = gfx::device->CreateBuffer(&desc, &init_data, &buf->buffer);
    }
    else {
        hr = gfx::device->CreateBuffer(&desc, nullptr, &buf->buffer);
    }

    if (FAILED(hr)) {
        err("couldn't create structured buffer");
        return false;
    }

    if (bind & Bind::GpuRead) {
        D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
        mem::zero(srv_desc);
        srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
        srv_desc.Format = DXGI_FORMAT_UNKNOWN;
        srv_desc.BufferEx.NumElements = (UINT)count;

        hr = gfx::device->CreateShaderResourceView(buf->buffer, &srv_desc, &buf->srv);

        if (FAILED(hr)) {
            err("couldn't create structured buffer's SRV");
            return false;
        }
    }

    if (bind & Bind::GpuWrite) {
        D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
        mem::zero(uav_desc);
        uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        uav_desc.Format = DXGI_FORMAT_UNKNOWN;
        uav_desc.Buffer.NumElements = (UINT)count;

        hr = gfx::device->CreateUnorderedAccessView(buf->buffer, &uav_desc, &buf->uav);

        if (FAILED(hr)) {
            err("couldn't create structured buffer's UAV");
            return false;
        }
    }

    return true;
}
