struct PixelInput {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORDS0;
};

cbuffer ShaderData : register(b0) {
	float3 cam_up;
	float time;
	float3 cam_fwd;
	float one_over_aspect_ratio;
	float3 cam_right;
	float cam_zoom;
	float3 cam_pos; 
	float padding__0;
};

struct BrushData {
	float3 pos;
	float radius;
	float3 norm;
	float padding__4;
};

struct Material {
	float3 albedo;
	float padding;
};

#define MAX_DIST 10000.

Texture3D<float> vol_tex : register(t0);
// Texture3D<uint2> material_tex : register(t1);
Texture3D<float3> material_tex : register(t1);
StructuredBuffer<BrushData> brush : register(t2);
StructuredBuffer<Material> materials : register(t3);

static float3 vol_tex_size = 0;
static float3 vol_tex_centre = 0;

// == scene functions ================================

float3 worldToTex(float3 world) {
	return world + vol_tex_centre;
}

float trilinearInterpolation(float3 pos, float3 size) {
    const int3 start = max(min(int3(pos), int3(size) - 2), 0);
    const int3 end = start + 1;

    const float3 delta = pos - start;
    const float3 rem = 1 - delta;

#define sample(x, y, z) vol_tex.Load(int4((x), (y), (z), 0))

	const float4 map_start = float4(
		sample(start.x, start.y, start.z),
		sample(start.x, end.y,   start.z),
		sample(start.x, start.y, end.z),
		sample(start.x, end.y,   end.z)
	);

	const float4 map_end = float4(
		sample(end.x, start.y, start.z),
		sample(end.x, end.y,   start.z),
		sample(end.x, start.y, end.z),  
		sample(end.x, end.y,   end.z)  
	);

	const float4 c = map_start * rem.x + map_end * delta.x;

#undef sample

    const float c0 = c.x * rem.y + c.y * delta.y;
    const float c1 = c.z * rem.y + c.w * delta.y;

    return c0 * rem.z + c1 * delta.z;
}

float preciseMap(float3 coords) {
	return trilinearInterpolation(coords, vol_tex_size);
}

float map(float3 coords) {
	return vol_tex.Load(int4(coords, 0));
}

bool isOutsideTexture(float3 pos) {
	return any(pos < 0) || any(pos >= vol_tex_size);
}

float3 calcNormal(float3 pos) {
	const float step = 3.;
	const float2 k = float2(1, -1);

	return normalize(
		k.xyy * preciseMap(pos + k.xyy * step) +
		k.yyx * preciseMap(pos + k.yyx * step) +
		k.yxy * preciseMap(pos + k.yxy * step) +
		k.xxx * preciseMap(pos + k.xxx * step)
	);
}

float getMouseDist(float3 pos) {
	// const float radius = 18.;
	return length(pos - brush[0].pos) - brush[0].radius;
}

// uint getMaterialIndex(float3 coords) {
// 	return material_tex.Load(int4(round(coords), 0));
// }

// float3 getAlbedo(uint index) {
// 	return materials[index].albedo;
// }

#define FIRST_INDEX_MASK    (255)
#define FIRST_INDEX_SHIFT   (0)
#define SECOND_INDEX_MASK   (65280)
#define SECOND_INDEX_SHIFT  (8)
// #define WEIGHT_MASK         (4294901760)
// #define WEIGHT_SHIFT        (16)
#define WEIGHT_MAX          (65536.)

#define GET_FIRST_INDEX(ind)  ((ind & FIRST_INDEX_MASK)  >> FIRST_INDEX_SHIFT)
#define GET_SECOND_INDEX(ind) ((ind & SECOND_INDEX_MASK) >> SECOND_INDEX_SHIFT)
// #define GET_WEIGHT(ind)       ((ind & WEIGHT_MASK)       >> WEIGHT_SHIFT)

// #define SECOND_INDEX_MASK   (16711680)
// #define SECOND_INDEX_SHIFT  (16)
// #define SECOND_WEIGHT_MASK  (4278190080)
// #define SECOND_WEIGHT_SHIFT (24)
// #define GET_FIRST_WEIGHT(ind)  ((ind & FIRST_WEIGHT_MASK)  >> FIRST_WEIGHT_SHIFT)
// #define GET_SECOND_WEIGHT(ind) ((ind & SECOND_WEIGHT_MASK) >> SECOND_WEIGHT_SHIFT)

float3 getAlbedo(float3 pos) {
	return material_tex.Load(int4(round(pos), 0));
	// const int3 coords = round(pos);
	// const uint2 mat_index = material_tex.Load(int4(coords, 0));
	// const uint first_index   = GET_FIRST_INDEX(mat_index.y);
	// const uint second_index  = GET_SECOND_INDEX(mat_index.y);
	// if (second_index) {
	// 	const uint weight_uint = mat_index.x;
	// 	const float weight = weight_uint / WEIGHT_MAX;
	// 	const float3 first_albedo = materials[first_index].albedo;
	// 	const float3 second_albedo = materials[second_index].albedo;
	// 	return lerp(second_albedo, first_albedo, weight);
	// }
	// else {
	// 	return materials[first_index].albedo;
	// }
}

float3 rayMarch(float3 ray_origin, float3 ray_dir) {
	float distance_traveled = 0;
	const int MAX_STEPS = 300;
	const float ROUGH_MIN_HIT_DISTANCE = 1;
	const float MIN_HIT_DISTANCE = .005;
	const float MAX_TRACE_DISTANCE = 1000;
	const int MAX_BOUNCES = 0;
	float3 current_pos;

	const float3 mouse_colour      = float3(.4, .4, .8);
	const float3 top_sky_colour    = float3(1, 0.1, 0.1);
	const float3 bottom_sky_colour = float3(0.1, 0.1, 1);

	const float3 light_pos = float3(sin(time) * 2, -5, cos(time) * 2) * 20.;
	const float3 light_dir = normalize(0 - light_pos);

	bool is_mouse = false;
	float3 final_colour = 1;
	int step_count = 0;
	int bounce_count = 0;

	for (; step_count < MAX_STEPS; ++step_count) {
		current_pos = ray_origin + ray_dir * distance_traveled;
		float closest = 0;

		float3 tex_pos = worldToTex(current_pos);
		if (isOutsideTexture(tex_pos)) {
			float distance = length(current_pos) - vol_tex_centre.x;
			closest = max(abs(distance), 1);
		}
		else {
			closest = map(tex_pos);
		}

		if (!is_mouse) {
			float mouse_close = getMouseDist(current_pos);
			// mouse brush doesn't have to be perfect, just has to give the idea of where the 
			// new brush will be added so lets use ROUGH_MIN_HIT_DISTANCE
			is_mouse = mouse_close < ROUGH_MIN_HIT_DISTANCE;
			closest = min(mouse_close, closest);
		}

		// first we use a rough check with a quick map function (does not use
		// trilinear filtering) to see if we're close enough to a surface, if
		// we are then we use a precise map function (with trilinear filtering,
		// meaning 8 texture lookups)

		if (closest < ROUGH_MIN_HIT_DISTANCE) {
			closest = preciseMap(tex_pos);

			if (closest < MIN_HIT_DISTANCE) {
				const float3 normal = calcNormal(tex_pos);
				// const float3 albedo = getAlbedo(getMaterialIndex(tex_pos));
				const float3 albedo = getAlbedo(tex_pos);
				// const float3 albedo = pow(normal * .5 + .5, 4.);

				float diffuse_intensity = max(0, dot(normal, light_dir));
				final_colour *= albedo * saturate(diffuse_intensity + 0.2);

				if (bounce_count >= MAX_BOUNCES) {
					break;
				}

				// reset values for ray_tracing
				bounce_count++;
				ray_dir = normalize(normal);
				ray_origin = current_pos + ray_dir;
				distance_traveled = 0;
			}
		}

		if (distance_traveled > MAX_TRACE_DISTANCE) {
			step_count = MAX_STEPS;
			break;
		}

		distance_traveled += closest;
	}

	if (step_count >= MAX_STEPS) {
		final_colour *= lerp(top_sky_colour, bottom_sky_colour, ray_dir.y * .5 + .5);
	}

	if (is_mouse) {
		final_colour *= mouse_colour;
	}

	return final_colour;
}

float4 main(PixelInput input) : SV_TARGET {
	vol_tex.GetDimensions(vol_tex_size.x, vol_tex_size.y, vol_tex_size.z);
	vol_tex_centre = vol_tex_size * 0.5;

	// convert to range (-1, 1)
	float2 uv = input.uv * 2.0 - 1.0;
	uv.y *= one_over_aspect_ratio;

	float3 ray_dir = normalize(cam_fwd + cam_right * uv.x + cam_up * uv.y);
	float3 ray_origin = cam_pos + cam_fwd * cam_zoom;

	float3 colour = rayMarch(ray_origin, ray_dir);
	
	return float4(sqrt(colour), 1.0);
}