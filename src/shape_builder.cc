#include "shape_builder.h"

#include <d3d11.h>

#include "system.h"
#include "tracelog.h"
#include "utils.h"

void ShapeBuilder::init() {
}

void ShapeBuilder::cleanup() {
	destroyBuffer();
}

void ShapeBuilder::bind() {
	if (dirty) {
		dirty = false;
		makeBuffer();
	}
	gfx::context->CSSetShaderResources(0, 1, &buf_srv);
}

void ShapeBuilder::unbind() {
	ID3D11ShaderResourceView *srv_nullptr[1] = { nullptr };
	gfx::context->CSSetShaderResources(0, 1, srv_nullptr);
}

void ShapeBuilder::addSphere(const vec3 &centre, float radius, Operations oper, Alteration alter) {
	Shape::ShapeData data;
	data.sphere.radius = radius;
	addShape(ShapeType::Sphere, oper, alter, centre, data);
}

void ShapeBuilder::addBox(const vec3 &centre, const vec3 &size, Operations oper, Alteration alter) {
	Shape::ShapeData data;
	data.box.size = size;
	addShape(ShapeType::Box, oper, alter, centre, data);
}

void ShapeBuilder::addCone(const vec3 &centre, const vec2 &angle_cos_sin, float height, Operations oper, Alteration alter) {
	Shape::ShapeData data;
	data.cone.angle_cos_sin = angle_cos_sin;
	data.cone.height = height;
	addShape(ShapeType::Cone, oper, alter, centre, data);
}

void ShapeBuilder::addCylinder(const vec3 &centre, const vec3 &bottom, const vec3 &top, float radius, Operations oper, Alteration alter) {
	Shape::ShapeData data;
	data.cylinder.bottom = bottom;
	data.cylinder.top = top;
	data.cylinder.radius = radius;
	addShape(ShapeType::Cylinder, oper, alter, centre, data);
}

void ShapeBuilder::addPyramid(const vec3 &centre, float height, Operations oper, Alteration alter) {
	Shape::ShapeData data;
	data.pyramid.height = height;
	addShape(ShapeType::Pyramid, oper, alter, centre, data);
}

void ShapeBuilder::destroyBuffer() {
	buffer.cleanup();
	safeRelease(buf_srv);
}

bool ShapeBuilder::makeBuffer() {
	if (shapes.empty()) {
		err("no shapes to make shapes buffer");
		return false;
	}

	HRESULT result = E_FAIL;

	// buffer is already large enough, just copy the data needed
	if (shapes.size() < buffer_cap) {
		if (Shape *data = buffer.map<Shape>()) {
			memcpy(data, shapes.data(), sizeof(Shape) * shapes.size());
			buffer.unmap();
		}
		else {
			err("couldn't map shape builder's buffer");
			return false;
		}

		return true;
	}

	destroyBuffer();

	D3D11_BUFFER_DESC buf_desc;
	mem::zero(buf_desc);
	buf_desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	buf_desc.ByteWidth = (UINT)(sizeof(ShapeType) * shapes.size());
	buf_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	buf_desc.StructureByteStride = sizeof(ShapeType);

	D3D11_SUBRESOURCE_DATA data;
	mem::zero(data);
	data.pSysMem = shapes.data();
	result = gfx::device->CreateBuffer(&buf_desc, &data, &buffer.buffer);

	if (FAILED(result)) {
		err("couldn't create shape builder's buffer");
		destroyBuffer();
		return false;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
	mem::zero(srv_desc);
	srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
	srv_desc.Format = DXGI_FORMAT_UNKNOWN;
	srv_desc.BufferEx.NumElements = (UINT)shapes.size();
	result = gfx::device->CreateShaderResourceView(buffer.buffer, &srv_desc, &buf_srv);

	if (FAILED(result)) {
		err("couldn't create shape builder's shader resource view");
		destroyBuffer();
		return false;
	}

	buffer_cap = shapes.size();
	return true;
}

void ShapeBuilder::addShape(ShapeType type, Operations oper, Alteration alter, const vec3 &centre, const Shape::ShapeData &data) {
	dirty = true;

	Shape shape;
	mem::zero(shape);
	shape.type = (uint32_t)type | (uint32_t)oper | (uint32_t)alter;
	shape.centre = centre;
	// TODO implement hierarchy
	shape.parent = 0;
	shape.as = data;
	shapes.emplace_back(shape);
}

#if 0
#include <d3d11.h>

#include "system.h"
#include "utils.h"
#include "macros.h"
#include "tracelog.h"

void ShapeBuilder::init() {
}

void ShapeBuilder::cleanup() {
	destroyBuffer();
}

void ShapeBuilder::bind() {
	if (dirty) {
		dirty = false;
		makeBuffer();
	}
	gfx::context->CSSetShaderResources(0, 1, &buf_srv);
}

void ShapeBuilder::unbind() {
	ID3D11ShaderResourceView *srv_nullptr[1] = { nullptr };
	gfx::context->CSSetShaderResources(0, 1, srv_nullptr);
}

void ShapeBuilder::addShape(vec3 centre, Shape shape, Operations oper, Alteration alter) {
	dirty = true;

	ShapeType new_shape;
	new_shape.shape = (uint32_t)shape;
	new_shape.shape|= (uint32_t)oper;
	new_shape.shape|= (uint32_t)alter;
	new_shape.centre = centre;
	shapes.emplace_back(new_shape);
}

void ShapeBuilder::destroyBuffer() {
	SAFE_RELEASE(buffer);
	SAFE_RELEASE(buf_srv);
}

bool ShapeBuilder::makeBuffer() {
	if (shapes.empty()) {
		err("no shapes to make shapes buffer");
		return false;
	}

	HRESULT result = E_FAIL;

	if (shapes.size() < buffer_cap) {
		// buffer is already large enough, just copy the data needed
		D3D11_MAPPED_SUBRESOURCE mapped;
		mem::zero(mapped);
		result = gfx::context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		if (FAILED(result)) {
			err("couldn't map shape builder's buffer");
			return false;
		}

		memcpy(mapped.pData, shapes.data(), sizeof(ShapeType) * shapes.size());

		gfx::context->Unmap(buffer, 0);

		return true;
	}

	destroyBuffer();

	D3D11_BUFFER_DESC buf_desc;
	mem::zero(buf_desc);
	buf_desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	buf_desc.ByteWidth = (UINT)(sizeof(ShapeType) * shapes.size());
	buf_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	buf_desc.StructureByteStride = sizeof(ShapeType);

	D3D11_SUBRESOURCE_DATA data;
	mem::zero(data);
	data.pSysMem = shapes.data();
	result = gfx::device->CreateBuffer(&buf_desc, &data, &buffer);

	if (FAILED(result)) {
		err("couldn't create shape builder's buffer");
		destroyBuffer();
		return false;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
	mem::zero(srv_desc);
	srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
	srv_desc.Format = DXGI_FORMAT_UNKNOWN;
	srv_desc.BufferEx.NumElements = (UINT)shapes.size();
	result = gfx::device->CreateShaderResourceView(buffer, &srv_desc, &buf_srv);

	if (FAILED(result)) {
		err("couldn't create shape builder's shader resource view");
		destroyBuffer();
		return false;
	}
}
#endif