#include "shaders/common.hlsl"

RWTexture3D<snorm float> tex : register(u0);

cbuffer ShapeData : register(b0) {
	float3 shape_pos;
	float padding;
    float4 data;
};

[numthreads(8, 8, 8)]
void main(uint3 id : SV_DispatchThreadID) {
    float3 size = 0;
    tex.GetDimensions(size.x, size.y, size.z);
    const float3 pos = id - size * 0.5;

#if   defined(SHAPE_SPHERE)
    tex[id] = min(sdf_sphere(pos, shape_pos, data.x) / MAX_STEP, 1);
#elif defined(SHAPE_BOX)
    tex[id] = min(sdf_box(pos, shape_pos, data.xyz) / MAX_STEP, 1);
#elif defined(SHAPE_CYLINDER)
    tex[id] = min(sdf_cylinder(pos, shape_pos, data.x, data.y) / MAX_STEP, 1);
#else
    tex[id] = 1;
#endif
}
