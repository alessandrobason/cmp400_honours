RWTexture3D<float> tex : register(u0);

cbuffer ShapeData : register(b0) {
    float4 data;
};

#define MAX_DIST 10000.

float sdf_sphere(float3 position, float radius) {
	return length(position) - radius;
}

float sdf_box(float3 position, float3 size) {
	float3 q = abs(position) - size;
	return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float sdf_cylinder(float3 position, float radius, float height) {
    float3 top = float3(0, height * 0.5, 0);
    float3 bottom = -top;

	float3 ba = top - bottom;
	float3 pa = position - bottom;
	float baba = dot(ba, ba);
	float paba = dot(pa, ba);
	float x = length(pa * baba - ba * paba) - radius * baba;
	float y = abs(paba - baba * 0.5) - baba * 0.5;
	float x2 = x * x;
	float y2 = y * y * baba;
	float d = (max(x, y) < 0.0) ? -min(x2, y2) : (((x > 0.0) ? x2 : 0.0) + ((y > 0.0) ? y2 : 0.0));
	return sign(d) * sqrt(abs(d)) / baba;
}

[numthreads(8, 8, 8)]
void main(uint3 id : SV_DispatchThreadID) {
    float3 size = 0;
    tex.GetDimensions(size.x, size.y, size.z);
    const float3 pos = id - size * 0.5;

#if   defined(SHAPE_SPHERE)
    tex[id] = sdf_sphere(pos, data.x);
#elif defined(SHAPE_BOX)
    tex[id] = sdf_box(pos, data.xyz);
#elif defined(SHAPE_CYLINDER)
    tex[id] = sdf_cylinder(pos, data.x, data.y);
#else
    tex[id] = MAX_DIST;
#endif
}
