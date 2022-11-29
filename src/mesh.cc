#include "mesh.h"

#include <d3d11.h>

#include "system.h"
#include "utils.h"
#include "tracelog.h"
#include "macros.h"

Mesh::Mesh(Mesh &&m) {
    *this = std::move(m);
}

Mesh::~Mesh() {
    cleanup();
}

Mesh &Mesh::operator=(Mesh &&m) {
    if (this != &m) {
        mem::copy(*this, m);
        mem::zero(m);
    }

    return *this;
}

bool Mesh::create(Slice<Vertex> vertices, Slice<Index> indices) {
    vertex_count = (int)vertices.len;
    index_count = (int)indices.len;
    
    D3D11_BUFFER_DESC vd;
    mem::zero(vd);
    vd.Usage = D3D11_USAGE_DEFAULT;
    vd.ByteWidth = (UINT)vertices.byteSize();
    vd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    
    D3D11_SUBRESOURCE_DATA vs;
    mem::zero(vs);
    vs.pSysMem = vertices.data;

    HRESULT hr = gfx::device->CreateBuffer(&vd, &vs, &vert_buf);
    if (FAILED(hr)) {
        err("couldn't create vertex buffer");
        return false;
    }

    D3D11_BUFFER_DESC id;
    mem::zero(id);
    id.Usage = D3D11_USAGE_DEFAULT;
    id.ByteWidth = (UINT)indices.byteSize();
    id.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA is;
    mem::zero(is);
    is.pSysMem = indices.data;

    hr = gfx::device->CreateBuffer(&id, &is, &ind_buf);
    if (FAILED(hr)) {
        err("couldn't create index buffer");
        return false;
    }

    return true;
}

void Mesh::cleanup() {
    SAFE_RELEASE(vert_buf);
    SAFE_RELEASE(ind_buf);
}

void Mesh::render() {
    UINT stride = sizeof(Vertex);
    UINT offset = 0;

    gfx::context->IASetVertexBuffers(0, 1, &vert_buf, &stride, &offset);
    gfx::context->IASetIndexBuffer(ind_buf, DXGI_FORMAT_R16_UINT, 0);
    gfx::context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    
    gfx::context->DrawIndexed(index_count, 0, 0);
}
