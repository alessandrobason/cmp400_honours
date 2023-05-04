#include "shaders/common.hlsl"

#define BASE_RADIUS 21.

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

Texture3D<float> vol_tex : register(t0);
RWStructuredBuffer<BrushData> brush : register(u0);
static float3 vol_tex_size = 0;

float3 worldToTex(float3 world) {
	return world + vol_tex_size / 2.;
}

float preciseMap(float3 coords) {
	return trilinearInterpolation(coords, vol_tex_size, vol_tex);
}

float roughMap(float3 coords) {
	return vol_tex.Load(int4(coords, 0));
}

float texBoundarySDF(float3 pos) {
	return sdf_box(pos, 0, vol_tex_size);
}

float3 calcNormal(float3 pos) {
	const float2 k = float2(1, -1);

	return normalize(
		k.xyy * preciseMap(pos + k.xyy * NORMAL_STEP) +
		k.yyx * preciseMap(pos + k.yyx * NORMAL_STEP) +
		k.yxy * preciseMap(pos + k.yxy * NORMAL_STEP) +
		k.xxx * preciseMap(pos + k.xxx * NORMAL_STEP)
	);
}

void rayMarch(float3 ro, float3 rd, out float3 out_normal, out float3 out_pos) {
	float distance_traveled = 0;
	const int NUMBER_OF_STEPS = 300;

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
    out_pos = pos + dir * 50000.;
}

[numthreads(1, 1, 1)]
void main() {
	vol_tex.GetDimensions(vol_tex_size.x, vol_tex_size.y, vol_tex_size.z);

    rayMarch(pos, dir, brush[0].norm, brush[0].pos);

	brush[0].pos += (-brush[0].norm) * (depth * BASE_RADIUS * scale);
	brush[0].radius = BASE_RADIUS * scale;
}