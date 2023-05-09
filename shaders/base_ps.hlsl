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
	bool use_tonemapping;
	float exposure_bias;
	float2 padding__0;
};

cbuffer Material : register(b1) {
	float3 albedo;
	bool use_texture;
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
Texture3D<snorm float> vol_tex     : register(t1);
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
	return vol_tex.Load(int4(round(coords), 0));
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

float getMouseDist(float3 pos) {
	return sdf_sphere(pos, brush[0].pos, brush[0].radius);
}

// from https://catlikecoding.com/unity/tutorials/advanced-rendering/triplanar-mapping/
float3 getTriplanarBlend(float3 pos, float3 normal) {
	const float3 blend = abs(normal);
	const float3 weights = blend / sum(blend);
	float3 tex_coords = pos / vol_tex_size;

	const float3 blend_r = material_tex.SampleLevel(tex_sampler, tex_coords.zy, 0).rgb * weights.x;
	const float3 blend_g = material_tex.SampleLevel(tex_sampler, tex_coords.xz, 0).rgb * weights.y;
	const float3 blend_b = material_tex.SampleLevel(tex_sampler, tex_coords.xy, 0).rgb * weights.z;

	return blend_r + blend_g + blend_b;
}

float3 getAlbedo(float3 pos, float3 normal) {
	float3 colour = albedo;
	if (use_texture) colour *= getTriplanarBlend(pos, normal);
	return colour;
}

float3 getBackground(float3 dir) {
	float2 uv;
	uv.x = 0.5 + atan2(dir.z, dir.x) / (2 * PI);
	uv.y = 0.5 - asin(dir.y) / PI;
	return background.SampleLevel(tex_sampler, uv, 0).rgb;
}

bool isInMouse(float3 ro, float3 rd, out float3 p) {
	// super small ray marcher to check if the current ray is in the
	// mouse brush, limit steps to 50 as we don't care too much
	// about accuracy
	float t = 0;
	for (int s = 0; s < 50; ++s) {
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
	float3 current_pos;

	const float3 mouse_colours[3] = {
		float3(.8, .4, .4), // outside the volume texture
		float3(.4, .8, .4), // outside the brush
		float3(.8, .8, .1), // inside the brush
	};

	const float3 light_pos = float3(sin(time) * 2, -5, cos(time) * 2) * 20.;
	const float3 light_dir = normalize(0 - light_pos);

	bool is_inside = false;
	float3 final_colour = 1;
	int step_count = 0;

	for (; step_count < MAX_STEPS; ++step_count) {
		current_pos = ray_origin + ray_dir * distance_traveled;
		float closest = texBoundarySDF(current_pos);
		uint light_id = num_of_lights;
		float light_dist = lightDistance(current_pos, light_id);

		if (light_dist < MIN_HIT_DISTANCE) {
			final_colour *= lights[light_id].colour;
			if (!is_inside) {
				final_colour = saturate(final_colour) - 0.5;
			}
			break;
		}

		// we're at least inside the texture
		if (closest < MIN_HIT_DISTANCE) {
			is_inside = true;
			float3 tex_pos = worldToTex(current_pos);
			tex_pos = clamp(tex_pos, 0, vol_tex_size - 1);
			// do a rough sample of the volume texture, this simply samples the closest voxel
			// without doing any filtering and is only used to avoid that expensive calculation
			// when we can
			closest = roughMap(tex_pos) * MAX_STEP;

			// if it is roughly close to a shape, do a much more precise check using trilinear filtering
			if (closest < ROUGH_MIN_HIT_DISTANCE) {
				closest = preciseMap(tex_pos) * MAX_STEP;

				if (closest < MIN_HIT_DISTANCE) {
					const float3 normal = calcNormal(tex_pos);
					const float3 albedo = getAlbedo(tex_pos, normal);
					const float diffuse_intensity = max(0, dot(normal, light_dir));
					const float ambient_intensity = 0.35;
					final_colour = albedo * saturate(diffuse_intensity + ambient_intensity);
					break;
				}
			}
		}

		if (distance_traveled > MAX_TRACE_DISTANCE) {
			step_count = MAX_STEPS;
			break;
		}

		distance_traveled += min(closest, light_dist);
	}

	if (step_count >= MAX_STEPS) {
		final_colour = getBackground(ray_dir);
		// remove some opacity if it is outside the texture
		final_colour = lerp(final_colour, 1, (!is_inside) * 0.5);
	}

	float3 mouse_pos = 0;
	if (isInMouse(ray_origin, ray_dir, mouse_pos)) {
		bool is_inside_tex = texBoundarySDF(mouse_pos) <= 0;

		float3 mouse_to_ro = mouse_pos - ray_origin;
		float3 surface_to_ro = current_pos - ray_origin;
		bool is_inside_shape = mag2(mouse_to_ro) > mag2(surface_to_ro);

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

	return float4(use_tonemapping ? toneMapping(colour, exposure_bias) : colour, 1.0);
}
