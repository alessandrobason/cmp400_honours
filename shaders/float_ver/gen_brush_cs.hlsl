RWTexture3D<float> brush : register(u0);

float sdf_sphere(float3 pos, float3 centre, float radius) {
	return length(pos - centre) - radius;
}

[numthreads(8, 8, 8)]
void main(uint3 id : SV_DispatchThreadID) {
    float width, h, d;
    brush.GetDimensions(width, h, d);
    brush[id] = sdf_sphere(float3(id) - width / 2., 0, width / 3.);
}