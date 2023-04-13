Texture3D<float> vol_tex : register(t0);

cbuffer FindData : register(b0) {
	float3 pos;
	float padding__0;
	float3 dir;
	float padding__1;
#if 0
	float2 rtv_pos;
	float2 rtv_size;
	float3 pos;
	float aspect_ratio;
	float3 fwd;
	float zoom;
	float3 right;
	float padding___1;
	float3 up;
	float padding___2;
	float2 mouse_pos;
	float2 padding___3;
#endif
};

cbuffer TexData : register(b1) {
    float3 tex_size;
    float padding__2;
	float3 tex_position;
	float padding__3;
};

struct BrushPosData {
	float3 brush_pos;
	float padding__4;
	float3 brush_norm;
	float padding__5;
};

RWStructuredBuffer<BrushPosData> brush_data;

float3 worldToTex(float3 world) {
	return world - tex_position + tex_size / 2.;
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
	return trilinearInterpolation(coords, tex_size);
}

float map(float3 coords) {
	return vol_tex.Load(int4(coords, 0));
}

float3 calcNormal(float3 pos) {
	const float step = 1;
	const float2 k = float2(1, -1);

	return normalize(
		k.xyy * map(pos + k.xyy * step) +
		k.yyx * map(pos + k.yyx * step) +
		k.yxy * map(pos + k.yxy * step) +
		k.xxx * map(pos + k.xxx * step)
	);
}

void rayMarch(float3 ro, float3 rd, out float3 out_normal, out float3 out_pos) {
	float distance_traveled = 0;
	const int NUMBER_OF_STEPS = 300;
	const float ROUGH_MIN_HIT_DISTANCE = 1;
	const float MIN_HIT_DISTANCE = .005;
	const float MAX_TRACE_DISTANCE = 1000;

	const float3 light_pos = float3(100, 100, 0);
	const float3 light_dir = normalize(0. - light_pos);

	float3 current_pos;

	for (int i = 0; i < NUMBER_OF_STEPS; ++i) {
		current_pos = ro + rd * distance_traveled;
		float closest = 0;

		// first we use a rough check with a quick map function (does not use
		// trilinear filtering) to see if we're close enough to a surface, if
		// we are then we use a precise map function (with trilinear filtering,
		// meaning 8 texture lookups)

		float3 tex_pos = worldToTex(current_pos);
		if (!all(tex_pos >= 0 && tex_pos < tex_size)) {
			float distance = length(current_pos - tex_position) - tex_size.x * 0.5;
			closest = max(abs(distance), 1);
		}
		else {
			closest = map(tex_pos);
		}

		if (closest < ROUGH_MIN_HIT_DISTANCE) {
			closest = preciseMap(tex_pos);

			if (closest < MIN_HIT_DISTANCE) {
				out_normal = calcNormal(tex_pos);
                out_pos = current_pos;
                return;
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
    rayMarch(
        pos, 
        dir, 
        brush_data[0].brush_norm,
        brush_data[0].brush_pos
    );
}