#define BASE_RADIUS 21.

Texture3D<int> vol_tex : register(t0);

cbuffer BrushFindData : register(b0) {
	float3 pos;
	float depth;
	float3 dir;
	float scale;
};

struct BrushData {
	float3 pos;
	float radius;
	float3 norm;
	float padding__5;
};

RWStructuredBuffer<BrushData> brush;
static float3 vol_tex_size = 0;

float3 worldToTex(float3 world) {
	return world + vol_tex_size / 2.;
}

float trilinearInterpolation(float3 pos, float3 size) {
    const int3 start = max(min(int3(pos), int3(size) - 2), 0);
    const int3 end = start + 1;

    const float3 delta = pos - start;
    const float3 rem = 1 - delta;

#define map(x, y, z) vol_tex.Load(int4((x), (y), (z), 0))

	const float4 map_start = float4(
		map(start.x, start.y, start.z),
		map(start.x, end.y,   start.z),
		map(start.x, start.y, end.z),
		map(start.x, end.y,   end.z)
	);

	const float4 map_end = float4(
		map(end.x, start.y, start.z),
		map(end.x, end.y,   start.z),
		map(end.x, start.y, end.z),  
		map(end.x, end.y,   end.z)  
	);

	const float4 c = map_start * rem.x + map_end * delta.x;

#undef map

    const float c0 = c.x * rem.y + c.y * delta.y;
    const float c1 = c.z * rem.y + c.w * delta.y;

    return c0 * rem.z + c1 * delta.z;
}

float preciseMap(float3 coords) {
	return trilinearInterpolation(coords, vol_tex_size) / 8.;
}

float roughMap(float3 coords) {
	return (float)vol_tex.Load(int4(coords, 0)) / 8.;
}

float texBoundarySDF(float3 pos) {
	float3 q = abs(pos) - vol_tex_size * 0.5;
	return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float3 calcNormal(float3 pos) {
	const float step = 1;
	const float2 k = float2(1, -1);

	return normalize(
		k.xyy * roughMap(pos + k.xyy * step) +
		k.yyx * roughMap(pos + k.yyx * step) +
		k.yxy * roughMap(pos + k.yxy * step) +
		k.xxx * roughMap(pos + k.xxx * step)
	);
}

void rayMarch(float3 ro, float3 rd, out float3 out_normal, out float3 out_pos) {
	float distance_traveled = 0;
	const int NUMBER_OF_STEPS = 300;
	const float ROUGH_MIN_HIT_DISTANCE = 1;
	const float MIN_HIT_DISTANCE = .005;
	const float MAX_TRACE_DISTANCE = 1000;

	float3 current_pos;

	for (int i = 0; i < NUMBER_OF_STEPS; ++i) {
		current_pos = ro + rd * distance_traveled;
		float closest = 0;
		closest = texBoundarySDF(current_pos);

		// we're at least inside the texture
		if (closest < MIN_HIT_DISTANCE) {
			float3 tex_pos = worldToTex(current_pos);
			// do a rough sample of the volume texture, this simply samples the closest voxel
			// without doing any filtering and is only used to avoid that expensive calculation
			// when we can
			closest = roughMap(tex_pos);

			// if it is roughly close to a shape, do a much more precise check using trilinear filtering
			if (closest < ROUGH_MIN_HIT_DISTANCE) {
				closest = preciseMap(tex_pos);

				if (closest < MIN_HIT_DISTANCE) {
					out_normal = calcNormal(tex_pos);
					out_pos = current_pos;
					return;
				}
			}
		}

		if (distance_traveled > MAX_TRACE_DISTANCE) {
			break;
		}

		distance_traveled += closest;
	}

    out_normal = 0;
    out_pos = pos + dir * 500.;
}

[numthreads(1, 1, 1)]
void main() {
	vol_tex.GetDimensions(vol_tex_size.x, vol_tex_size.y, vol_tex_size.z);

    rayMarch(
        pos, 
        dir, 
        brush[0].norm,
        brush[0].pos
    );

	brush[0].pos += (-brush[0].norm) * (depth * BASE_RADIUS * scale);
	brush[0].radius = BASE_RADIUS * scale;
}