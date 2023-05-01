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
	uint num_of_lights;
};

cbuffer Material : register(b1) {
	float3 albedo;
	uint texture_mode;
    float3 specular_colour;
    float smoothness;
    float3 emissive_colour;
    float specular_probability;
};

struct BrushData {
	float3 pos;
	float radius;
	float3 norm;
	float padding__4;
};

struct LightData {
    float3 pos;
    float radius;
    float3 colour;
    bool render;
};

StructuredBuffer<BrushData> brush  : register(t0);
Texture3D<float> vol_tex           : register(t1);
Texture2D material_tex             : register(t2);
Texture2D background               : register(t3);
StructuredBuffer<LightData> lights : register(t4);

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
    return sdf_box(pos, 0, vol_tex_size);
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

float3 getSphereCoordsBlend(float3 normal) {
	float2 uv;
	uv.x = atan2(normal.x, normal.z) / (2 * PI) + 0.5;
	uv.y = -(normal.y * 0.5 + 0.5);
	return material_tex.SampleLevel(tex_sampler, uv, 0).rgb;
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
	return background.SampleLevel(tex_sampler, uv, 0).rgb;
}

bool isInMouse(float3 ro, float3 rd, out float3 p) {
	const int MAX_STEPS = 50;
	const float MAX_TRACE_DISTANCE = 3000;
	float t = 0;
	for (int s = 0; s < MAX_STEPS; ++s) {
		p = ro + rd * t;
		float d = getMouseDist(p);
		if (d <= .1) return true;
		t += d;
		if (d > MAX_TRACE_DISTANCE) break;
	}
	return false;
}

float lightDistance(float3 pos, out uint light_id) {
    float dist = MAX_STEP;
    for (uint i = 0; i < num_of_lights; ++i) {
        LightData light = lights[i];
        if (light.render) {
            float light_dist = sdf_sphere(pos, light.pos, light.radius);
            if (light_dist < dist) {
                dist = light_dist;
                light_id = i;
            }
        }
    }
    return dist;
}

float3 rayMarch(float3 ray_origin, float3 ray_dir) {
	float distance_traveled = 0;
	const int MAX_STEPS = 500;
	const float ROUGH_MIN_HIT_DISTANCE = 0.05;
	const float MIN_HIT_DISTANCE = .005;
	const float MAX_TRACE_DISTANCE = 3000;
	const int MAX_BOUNCES = 0;
	float3 current_pos;

	const float3 mouse_colours[3] = {
		float3(.8, .4, .4), // outside the volume texture
		float3(.4, .8, .4), // outside the brush
		float3(.8, .8, .1), // inside the brush
	};

	const float3 inside_mouse_colour  = float3(.4, .4, .8);
	const float3 outside_mouse_colour = float3(.8, .4, .4);
	const float3 top_sky_colour    = float3(1, 0.1, 0.1);
	const float3 bottom_sky_colour = float3(0.1, 0.1, 1);

	const float3 light_pos = float3(sin(time) * 2, -5, cos(time) * 2) * 20.;
	const float3 light_dir = normalize(0 - light_pos);

	bool is_inside = false;
	float3 final_colour = 1;
	int step_count = 0;
	int bounce_count = 0;

	for (; step_count < MAX_STEPS; ++step_count) {
		current_pos = ray_origin + ray_dir * distance_traveled;
		float closest = texBoundarySDF(current_pos);
		uint light_id = num_of_lights;
		float light_dist = lightDistance(current_pos, light_id);

		if (light_dist < MIN_HIT_DISTANCE) {
			final_colour *= saturate(lights[light_id].colour) - (!is_inside * 0.8);
			break;
		}

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
					const float ambient_intensity = 0.35;
					// const float ambient_occlusion = (float)step_count / MAX_STEPS;
					final_colour *= albedo * saturate(diffuse_intensity + ambient_intensity);
					break;
				}
			}
		}

		if (distance_traveled > MAX_TRACE_DISTANCE) {
			step_count = MAX_STEPS;
			if (!is_inside) {
				final_colour -= 0.8;
			}
			break;
		}

		distance_traveled += min(closest, light_dist);
	}

	if (step_count >= MAX_STEPS) {
		final_colour *= getBackground(ray_dir);
	}

	float3 mouse_pos = 0;
	if (isInMouse(ray_origin, ray_dir, mouse_pos)) {
		bool is_inside_tex = texBoundarySDF(mouse_pos) <= 0;

		float3 mouse_to_ro = mouse_pos - ray_origin;
		float3 surface_to_ro = current_pos - ray_origin;
		bool is_inside_shape = dot(mouse_to_ro, mouse_to_ro) > dot(surface_to_ro, surface_to_ro);

		final_colour *= mouse_colours[(int)is_inside_tex + is_inside_shape];
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

	// if (all(colour < 0)) {
	// 	return 0;
	// }

	// return float4(colour, 1.0);
	return float4((colour), 1.0);
}
