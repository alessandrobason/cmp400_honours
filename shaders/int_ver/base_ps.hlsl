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

cbuffer Material : register(b1) {
	float3 albedo;
	int has_texture;
};

struct BrushData {
	float3 pos;
	float radius;
	float3 norm;
	float padding__4;
};

#define MAX_DIST 10000.
#define PI 3.14159265

StructuredBuffer<BrushData> brush : register(t0);
Texture3D<int> vol_tex : register(t1);
Texture2D material_tex : register(t2);
Texture2D bg_hdr : register(t3);
Texture2D irradiance_map : register(t4);

sampler tex_sampler;

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

#define sample(x, y, z) (vol_tex.Load(int4((x), (y), (z), 0)))

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
	return length(pos - brush[0].pos) - brush[0].radius;
}

float3 getTriplanarBlend(float3 pos, float3 normal) {
	const float3 blend = abs(normal);
	const float3 weights = blend / dot(blend, 1.);
	float3 tex_coords = pos / vol_tex_size;

	const float3 blend_r = material_tex.SampleLevel(tex_sampler, tex_coords.yz, 0).rgb * weights.x;
	const float3 blend_g = material_tex.SampleLevel(tex_sampler, tex_coords.xz, 0).rgb * weights.y;
	const float3 blend_b = material_tex.SampleLevel(tex_sampler, tex_coords.xy, 0).rgb * weights.z;

	return blend_r + blend_g + blend_b;
}

float3 getAlbedo(float3 pos, float3 normal) {
	float3 colour = albedo;
	if (has_texture) colour *= getTriplanarBlend(pos, normal);
	return colour;
}

float3 getBackground(float3 dir) {
	//float2 uv = float2(
	//	atan2(dir.x, dir.z) / (2 * PI) + 0.5,
	//	-(dir.y * 0.5 + 0.5)
	//);
	float2 uv;
	uv.x = 0.5 + atan2(dir.z, dir.x) / (2 * PI);
	uv.y = 0.5 - asin(dir.y) / PI;
	return bg_hdr.SampleLevel(tex_sampler, uv, 0).rgb;
}

float3 getIrradiance(float3 dir) {
	float2 uv;
	uv.x = atan2(dir.x, dir.z) / (2 * PI) + 0.5;
	uv.y = -(dir.y * 0.5 + 0.5);
	return irradiance_map.SampleLevel(tex_sampler, uv, 0).rgb;
}

float3 fresnelSchlickRoughness(float cosTheta, float3 F0, float roughness) {
    return F0 + (max(1.0 - roughness, F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}   

float3 rayMarch(float3 ray_origin, float3 ray_dir) {
	float distance_traveled = 0;
	const int MAX_STEPS = 500;
	const float ROUGH_MIN_HIT_DISTANCE = 0.5;
	const float MIN_HIT_DISTANCE = .005;
	const float MAX_TRACE_DISTANCE = 3000;
	const int MAX_BOUNCES = 0;
	float3 current_pos;

	const float3 inside_mouse_colour  = float3(.4, .4, .8);
	const float3 outside_mouse_colour = float3(.8, .4, .4);
	const float3 top_sky_colour    = float3(1, 0.1, 0.1);
	const float3 bottom_sky_colour = float3(0.1, 0.1, 1);

	const float3 light_pos = float3(sin(time) * 2, -5, cos(time) * 2) * 20.;
	const float3 light_dir = normalize(0 - light_pos);

	bool is_mouse = false;
	bool is_inside = false;
	float3 final_colour = 1;
	int step_count = 0;
	int bounce_count = 0;

	for (; step_count < MAX_STEPS; ++step_count) {
		current_pos = ray_origin + ray_dir * distance_traveled;
		float closest = 0;
		closest = texBoundarySDF(current_pos);

		// we're at least inside the texture
		if (closest < MIN_HIT_DISTANCE) {
			is_inside = true;
			float3 tex_pos = worldToTex(current_pos);
			// do a rough sample of the volume texture, this simply samples the closest voxel
			// without doing any filtering and is only used to avoid that expensive calculation
			// when we can
			closest = roughMap(tex_pos);

			// if it is roughly close to a shape, do a much more precise check using trilinear filtering
			if (closest < ROUGH_MIN_HIT_DISTANCE) {
				closest = preciseMap(tex_pos);

				if (closest < MIN_HIT_DISTANCE) {
					const float3 normal = calcNormal(tex_pos);
					const float3 albedo = getAlbedo(tex_pos, normal);
					const float diffuse_intensity = max(0, dot(normal, light_dir));
					const float ambient_intensity = 0.05;
					// const float ambient_occlusion = (float)step_count / MAX_STEPS;
					final_colour *= albedo * saturate(diffuse_intensity + ambient_intensity);
					break;
				}
			}
		}

		if (!is_mouse) {
			float mouse_close = getMouseDist(current_pos);
			// mouse brush doesn't have to be perfect, just has to give the idea of where the 
			// new brush will be added so lets use ROUGH_MIN_HIT_DISTANCE
			is_mouse = mouse_close < ROUGH_MIN_HIT_DISTANCE;
			closest = min(mouse_close, closest);
		}

		if (distance_traveled > MAX_TRACE_DISTANCE) {
			step_count = MAX_STEPS;
			if (!is_inside) {
				final_colour -= 0.8;
			}
			break;
		}

		distance_traveled += closest;
	}

	if (step_count >= MAX_STEPS) {
		final_colour *= getBackground(ray_dir);
	}

	if (is_mouse) {
		final_colour *= is_inside ? inside_mouse_colour : outside_mouse_colour;
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
