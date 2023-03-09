RWTexture3D<float> tex : register(u0);

float sdf_sphere(float3 p, float3 c, float r) {
    return length(p - c) - r;
}

[numthreads(8, 8, 8)]
void main(uint3 id : SV_DispatchThreadID) {
    tex[id] = sdf_sphere(float3(id) - 512 / 2., 0, 64 / 2.);
}
