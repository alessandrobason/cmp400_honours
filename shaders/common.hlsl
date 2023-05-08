#define MAX_STEP 128
#define PI 3.14159265
#define ROUGH_MIN_HIT_DISTANCE 1.
#define MIN_HIT_DISTANCE .005
#define MAX_TRACE_DISTANCE 3000
#define NORMAL_STEP 3.

#define mag2(v) (dot((v), (v)))
// sums together all the values in a vector
#define sum(v)  (dot((v), 1.))

float trilinearInterpolation(float3 pos, float3 size, Texture3D<snorm float> tex) {
    int3 start = max(min(int3(pos), int3(size) - 2), 0);
    int3 end = start + 1;

    float3 delta = pos - start;
    float3 rem = 1 - delta;

#define map(x, y, z) tex.Load(int4((x), (y), (z), 0))

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

float sdf_sphere(float3 pos, float3 centre, float r) {
	return length(pos - centre) - r;
}

float sdf_box(float3 pos, float3 centre, float3 s) {
	float3 q = abs(pos - centre) - s * 0.5;
	return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float sdf_cylinder(float3 position, float3 centre, float radius, float height) {
	position -= centre;
	float2 d = abs(float2(length(position.xz),position.y)) - float2(radius, height);
	return min(max(d.x,d.y),0.0) + length(max(d,0.0));
}

float3 Uncharted2Tonemap(float3 x) {
	static const float A = 0.15;
	static const float B = 0.50;
	static const float C = 0.10;
	static const float D = 0.20;
	static const float E = 0.02;
	static const float F = 0.30;

   	return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

// from http://filmicworlds.com/blog/filmic-tonemapping-operators/
float3 toneMapping(float3 colour, float exposure_bias) {
	colour = Uncharted2Tonemap(colour * exposure_bias);
	
	static const float W = 11.2;
	float3 white_scale = 1.0 / Uncharted2Tonemap(W);
	colour = colour * white_scale;
	colour = pow(colour, 1/2.2);
	return colour;
}