RWTexture3D<float> brush : register(u0);

#define BRUSH_SZ 64.

float sdf_sphere(float3 pos, float3 centre, float radius) {
	return length(pos - centre) - radius;
}

[numthreads(8, 8, 8)]
void main(uint3 id : SV_DispatchThreadID) {
    brush[id] = sdf_sphere(float3(id) - BRUSH_SZ / 2., 0, BRUSH_SZ / 3.);
}