Texture3D<float> source : register(t0);
RWTexture3D<float> destination : register(u0);

static float3 src_size;
static float3 dst_size;

float trilinearInterpolation(float3 pos) {
    int3 start = max(min(int3(pos), int3(src_size) - 2), 0);
    int3 end = start + 1;

    float3 delta = pos - start;
    float3 rem = 1 - delta;

#define map(x, y, z) source.Load(int4((x), (y), (z), 0))

    float4 c = float4(
        map(start.x, start.y, start.z) * rem.x + map(end.x, start.y, start.z) * delta.x,
        map(start.x, end.y,   start.z) * rem.x + map(end.x, end.y,   start.z) * delta.x,
        map(start.x, start.y, end.z)   * rem.x + map(end.x, start.y, end.z)   * delta.x,
        map(start.x, end.y,   end.z)   * rem.x + map(end.x, end.y,   end.z)   * delta.x
    );

#undef map

    float c0 = c.x * rem.y + c.y * delta.y;
    float c1 = c.z * rem.y + c.w * delta.y;

    return c0 * rem.z + c1 * delta.z;
}

inline float sampleSource(float3 position, float3 scale) {
    return trilinearInterpolation(position / scale);
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