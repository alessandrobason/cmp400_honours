#include "shaders/common.hlsl"

Texture3D<float> source : register(t0);
RWTexture3D<float> destination : register(u0);

static float3 src_size;
static float3 dst_size;

inline float sampleSource(float3 position, float3 scale) {
    return trilinearInterpolation(position / scale, src_size, source);
}

inline float3 idToWorld(uint3 id) {
    return float3(id) - dst_size * 0.5;
}

[numthreads(8, 8, 8)]
void main(uint3 id : SV_DispatchThreadID) {
    source.GetDimensions(src_size.x, src_size.y, src_size.z);
    destination.GetDimensions(dst_size.x, dst_size.y, dst_size.z);

    const float3 scale = dst_size / src_size;
    destination[id] = sampleSource(id, scale);
}