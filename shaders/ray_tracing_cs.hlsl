#include "shaders/common.hlsl"

cbuffer RayTraceData : register(b0) {
    uint2 thread_loc;
    uint num_rendered_frames;
    uint num_of_lights;
	uint maximum_steps;
	uint maximum_bounces;
	uint maximum_rays;
	float maximum_trace_dist;
};

cbuffer ShaderData : register(b1) {
	float3 cam_up;
	float time;
	float3 cam_fwd;
	float one_over_aspect_ratio;
	float3 cam_right;
	float cam_zoom;
	float3 cam_pos; 
	float padding__0;
	bool use_tonemapping;
	float3 padding__1;
};

cbuffer Material : register(b2) {
	float3 albedo;
	bool use_texture;
    float3 specular_colour;
    float smoothness;
    float3 emissive_colour;
    float specular_probability;
};

struct LightData {
    float3 pos;
    float radius;
    float3 colour;
    bool render;
};

RWTexture2D<unorm float4> output   : register(u0);

Texture3D<float> vol_tex           : register(t0);
Texture2D diffuse_tex              : register(t1);
Texture2D background               : register(t2);
StructuredBuffer<LightData> lights : register(t3);

sampler tex_sampler;

static float3 vol_tex_size   = 0;
static float3 vol_tex_centre = 0;

#define TRIPLANAR_BLEND (1)
#define SPHERE_COODS    (2)

// == scene functions ================================

float3 getPrevious(uint2 id) {
    return output[id].rgb;
}

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

float lightDistance(float3 pos, int bounce, out uint light_id) {
    float dist = MAX_STEP;
    for (uint i = 0; i < num_of_lights; ++i) {
        LightData light = lights[i];
        if (light.render || bounce) {
            float light_dist = sdf_sphere(pos, light.pos, light.radius);
            if (light_dist < dist) {
                dist = light_dist;
                light_id = i;
            }
        }
    }
    return dist;
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

float3 lightNormal(float3 pos, float3 c, float r) {
	return normalize(pos - c);
}

// from https://catlikecoding.com/unity/tutorials/advanced-rendering/triplanar-mapping/
float3 getTriplanarBlend(float3 pos, float3 normal) {
	const float3 blend = abs(normal);
	const float3 weights = blend / sum(blend);
	float3 tex_coords = pos / vol_tex_size;

	const float3 blend_r = diffuse_tex.SampleLevel(tex_sampler, tex_coords.zy, 0).rgb * weights.x;
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
	if (use_texture) colour *= getTriplanarBlend(pos, normal);
	return colour;
}

float3 getEnvironment(float3 rd) {
	float2 uv;
	uv.x = 0.5 + atan2(rd.z, rd.x) / (2 * PI);
	uv.y = 0.5 - asin(rd.y) / PI;
	return background.SampleLevel(tex_sampler, uv, 0).rgb;
}

// == random functions ===============================

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

float2 randomPointInCircle(inout uint state) {
    float angle = random(state) * 2. * PI;
    float2 point_on_circle = float2(cos(angle), sin(angle));
    return point_on_circle * sqrt(random(state));
}

// == ray tracing ====================================

struct Material {
	float3 albedo;
	float3 light;
};

struct HitInfo {
	float3 normal;
	float3 position;
	Material material;
};

HitInfo rayMarch(float3 ro, float3 rd, int bounce, inout uint state) {
	HitInfo info;
	info.normal   = 0;
	info.position = 0;
	info.material.albedo = 0;
	info.material.light  = 0;

	float distance_traveled = 0;

	for (int step_count = 0; step_count < maximum_steps; ++step_count) {
		float3 current_pos = ro + rd * distance_traveled;
		float closest = texBoundarySDF(current_pos);
        uint light_id = num_of_lights;
        float light_dist = lightDistance(current_pos, bounce, light_id);

		if (light_dist < MIN_HIT_DISTANCE) {
            LightData light = lights[light_id];
            info.normal          = lightNormal(current_pos, light.pos, light.radius);
            info.position        = current_pos;
            info.material.albedo = 0;
            info.material.light  = light.colour;
            break;
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
                    info.normal          = calcNormal(tex_pos);
                    info.position        = current_pos;
                    info.material.albedo = getAlbedo(tex_pos, info.normal);
	                info.material.light  = emissive_colour;
                    break;
				}
			}
		}

		if (distance_traveled > maximum_trace_dist) {
			break;
		}

		distance_traveled += min(closest, light_dist);
	}

	return info;
}

float3 rayTrace(float3 ro, float3 rd, inout uint state) {
	float3 incoming_light = 0;
	float3 ray_colour = 1;

	for (int bounce = 0; bounce <= maximum_bounces; ++bounce) {
		HitInfo info = rayMarch(ro, rd, bounce, state);
		
		if (any(info.normal != 0)) {
			ro = info.position + info.normal;
            float3 diffuse_dir = normalize(info.normal + randomDir(state));
            float3 specular_dir = reflect(rd, info.normal);
            bool is_specular_bounce = specular_probability >= random(state);
            rd = lerp(diffuse_dir, specular_dir, smoothness * is_specular_bounce);

            incoming_light += info.material.light * ray_colour;
		    ray_colour *= lerp(info.material.albedo, specular_colour, is_specular_bounce);
		}
		// no hit
		else {
            incoming_light += getEnvironment(rd) * ray_colour;
			break;
		}
	}

	return incoming_light;
}

// == main ===========================================

[numthreads(8, 8, 1)]
void main(uint2 thread_id : SV_DispatchThreadID) {
    uint2 id = thread_id + thread_loc;

	uint2 tex_size;
	output.GetDimensions(tex_size.x, tex_size.y);

    if (any(id > tex_size)) return;

	vol_tex.GetDimensions(vol_tex_size.x, vol_tex_size.y, vol_tex_size.z);
	vol_tex_centre = vol_tex_size * 0.5;

    float2 tex_uv = (float2)id / tex_size;
    tex_uv.y = 1. - tex_uv.y;

	// convert to range (-1, 1)
	float2 uv = tex_uv * 2.0 - 1.0;
	uv.y *= one_over_aspect_ratio;
	uint rng_state = id.y * tex_size.x + id.x + num_rendered_frames * 12345;

	float3 total_light = 0;

    float3 ray_origin = cam_pos + cam_fwd * cam_zoom;
    float3 focus_point = cam_fwd + cam_right * uv.x + cam_up * uv.y;

	for (int ray = 0; ray < maximum_rays; ++ray) {
        float2 aa_jitter = randomPointInCircle(rng_state) * 0.001;
        float3 aa_focus_point = focus_point + cam_right * aa_jitter.x + cam_up * aa_jitter.y;
        float3 ray_dir = normalize(aa_focus_point);
		
        total_light += rayTrace(ray_origin, ray_dir, rng_state);
	}

	float3 colour = total_light / maximum_rays;
	if (use_tonemapping) {
		colour = toneMapping(colour);
	}

    if (num_rendered_frames > 0) {
        float3 previous_col = output[id].rgb;
        colour = lerp(previous_col, colour, 1.0 / (num_rendered_frames + 1));
    }

    output[id] = float4(colour, 1);
}