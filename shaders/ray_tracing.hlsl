#include "shaders/common.hlsl"

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
	uint texture_mode;
};

Texture3D<float> vol_tex	: register(t0);
Texture2D diffuse_tex 		: register(t1);
Texture2D bg_hdr 			: register(t2);
Texture2D irradiance_map	: register(t3);
Texture2D previous 			: register(t4);

sampler tex_sampler;

static float3 vol_tex_size = 0;
static float3 vol_tex_centre = 0;

#define TRIPLANAR_BLEND (1)
#define SPHERE_COODS    (2)

// == scene functions ================================

float3 worldToTex(float3 world) {
	return world + vol_tex_centre;
}

float preciseMap(float3 coords) {
	return trilinearInterpolation(coords, vol_tex_size, vol_tex);
}

float roughMap(float3 coords) {
	return vol_tex.Load(int4(coords, 0));
}

float texBoundarySDF(float3 pos) {
	float3 q = abs(pos) - vol_tex_size * 0.5;
	return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float sdf_sphere(float3 pos, float3 c, float r) {
	return length(pos - c) - r;
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

float3 lightNormal(float3 pos, float3 c, float r) {
	const float step = 0.1;
	const float2 k = float2(1, -1);

	return normalize(
		k.xyy * sdf_sphere(pos + k.xyy * step, c, r) +
		k.yyx * sdf_sphere(pos + k.yyx * step, c, r) +
		k.yxy * sdf_sphere(pos + k.yxy * step, c, r) +
		k.xxx * sdf_sphere(pos + k.xxx * step, c, r)
	);
}

float3 getTriplanarBlend(float3 pos, float3 normal) {
	const float3 blend = abs(normal);
	const float3 weights = blend / dot(blend, 1.);
	float3 tex_coords = pos / vol_tex_size;

	const float3 blend_r = diffuse_tex.SampleLevel(tex_sampler, tex_coords.yz, 0).rgb * weights.x;
	const float3 blend_g = diffuse_tex.SampleLevel(tex_sampler, tex_coords.xz, 0).rgb * weights.y;
	const float3 blend_b = diffuse_tex.SampleLevel(tex_sampler, tex_coords.xy, 0).rgb * weights.z;

	return blend_r + blend_g + blend_b;
}

float3 getSphereCoordsBlend(float3 normal) {
	float2 uv;
	uv.x = atan2(normal.x, normal.z) / (2 * PI) + 0.5;
	uv.y = -(normal.y * 0.5 + 0.5);
	return diffuse_tex.SampleLevel(tex_sampler, uv, 0).rgb;
}

float3 getAlbedo(float3 pos, float3 normal) {
	float3 colour = albedo;
	switch (texture_mode) {
		case TRIPLANAR_BLEND: colour *= getTriplanarBlend(pos, normal); break;
		case SPHERE_COODS:    colour *= getSphereCoordsBlend(normal);   break;
	}
	return colour;
}

float3 getBackground(float3 dir) {
	float2 uv;
	uv.x = 0.5 + atan2(dir.z, dir.x) / (2 * PI);
	uv.y = 0.5 - asin(dir.y) / PI;
	return bg_hdr.SampleLevel(tex_sampler, uv, 0).rgb;
}

float random(inout uint state) {
	state = state * 747796405 + 2891336453;
	uint result = ((state >> ((state >> 28) + 4)) ^ state) * 277803737;
	result = (result >> 22) ^ result;
	return result / 4294967295.0;
}

float randomNormDistribution(inout uint state) {
	float theta = 2 * PI * random(state);
	float rho = sqrt(-2 * log(random(state)));
	return rho * cos(theta);
}

float3 randomDir(inout uint state) {
	return normalize(float3(
		randomNormDistribution(state),
		randomNormDistribution(state),
		randomNormDistribution(state)
	));
}

float3 randomHemisphereDir(float3 norm, inout uint state) {
	float3 dir = randomDir(state);
	return dir * sign(dot(norm, dir));
}

struct Material {
	float3 albedo;
	float3 light;
};

struct HitInfo {
	// float3 normal;
	// float3 position;
	// Material material;

	float3 incoming_light;
	float3 ray_colour;
};

HitInfo rayMarch(float3 ro, float3 rd, inout uint state) {
	HitInfo info;
	info.incoming_light = 0;
	info.ray_colour = 1;
	// info.normal   = 0;
	// info.position = 0;
	// info.material.albedo = 0;
	// info.material.light  = 0;

	float distance_traveled = 0;
	const int MAX_STEPS = 100;
	const int MAX_BOUNCES = 1;
	const float ROUGH_MIN_HIT_DISTANCE = 0.05;
	const float MIN_HIT_DISTANCE = .001;
	const float MAX_TRACE_DISTANCE = 600;

	const float3 light_pos = float3(-400, 0, 0);
	const float light_radius = 150;
	const float3 light_colour = 0;
	const float3 light_emissive = 1;

	int bounce = 0;

	for (int step_count = 0; step_count < MAX_STEPS; ++step_count) {
		float3 current_pos = ro + rd * distance_traveled;
		float closest = texBoundarySDF(current_pos);
		float light_dist = sdf_sphere(current_pos, light_pos, light_radius);

		if (light_dist < MIN_HIT_DISTANCE) {
			float3 normal = lightNormal(current_pos, light_pos, light_radius);

			info.incoming_light += light_emissive * info.ray_colour;
			info.ray_colour *= light_colour;

			if (bounce++ >= MAX_BOUNCES) break;

			rd = randomHemisphereDir(normal, state);
			ro = current_pos + normal;
			distance_traveled = 0;
		}

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
					float3 normal = calcNormal(tex_pos);
					float3 albedo = getAlbedo(tex_pos, normal);
					float3 emissive = 0;

					info.incoming_light += emissive * info.ray_colour;
					info.ray_colour *= albedo;

					if (bounce++ >= MAX_BOUNCES) break;

					rd = randomHemisphereDir(normal, state);
					ro = current_pos + normal;
					distance_traveled = 0;
				}
			}
		}

		if (distance_traveled > MAX_TRACE_DISTANCE) {
			break;
		}

		distance_traveled += min(closest, light_dist);
	}

	return info;
}

float3 rayTrace(float3 ro, float3 rd, inout uint state) {
#if 0
	const int MAX_BOUNCES = 0;

	float3 incoming_light = 0;
	float3 ray_colour = 1;

	for (int bounce = 0; bounce <= MAX_BOUNCES; ++bounce) {
		HitInfo info = rayMarch(ro, rd, state);
		incoming_light += info.material.light * ray_colour;
		ray_colour *= info.material.albedo;
		
		if (any(info.normal != 0)) {
			rd = randomHemisphereDir(info.normal, state);
			ro = info.position + info.normal;
		}
		// no hit
		else {
			break;
		}
	}

	return incoming_light;
#endif
	HitInfo info = rayMarch(ro, rd, state);
	return info.incoming_light;
}

float4 main(PixelInput input) : SV_TARGET {
	vol_tex.GetDimensions(vol_tex_size.x, vol_tex_size.y, vol_tex_size.z);
	vol_tex_centre = vol_tex_size * 0.5;

	uint2 rt_size;
	previous.GetDimensions(rt_size.x, rt_size.y);

	// convert to range (-1, 1)
	float2 uv = input.uv * 2.0 - 1.0;
	uv.y *= one_over_aspect_ratio;

	float3 ray_dir = normalize(cam_fwd + cam_right * uv.x + cam_up * uv.y);
	float3 ray_origin = cam_pos + cam_fwd * cam_zoom;

	uint2 pixel_coord = input.uv * rt_size;
	uint rng_state = pixel_coord.y * rt_size.x + pixel_coord.x;

	const int MAX_RAYS = 10;
	float3 total_light = 0;

	for (int ray = 0; ray < MAX_RAYS; ++ray) {
		total_light += rayTrace(ray_origin, ray_dir, rng_state);
	}

	float3 colour = total_light / MAX_RAYS;
	//float3 previous_colour = previous.SampleLevel(tex_sampler, input.uv, 0).rgb;
	
	//return float4(lerp(colour, previous_colour, 0.5), 1.0);
	return float4(colour, 1);
}
