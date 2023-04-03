struct PixelInput {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORDS0;
};

cbuffer ShaderData : register(b0) {
	float3 cam_up;
	float time;
	float3 cam_fwd;
	float img_width;
	float3 cam_right;
	float img_height;
	float3 cam_pos; 
	float cam_zoom;
};

cbuffer TexData : register(b1) {
    float3 tex_size;
    float padding__1;
	float3 tex_position;
	float padding__2;
};

Texture3D<float> vol_tex : register(t0);
sampler sampler_0;

// == scene functions ================================

float3 worldToTex(float3 world) {
	return world - tex_position + tex_size / 2.;
}

float map(float3 coords) {
	return vol_tex.Load(int4(coords, 0));
}

bool isInsideTexture(float3 pos) {
	return all(pos >= 0 && pos < 512);
}

float3 calcNormal(float3 pos) {
	const float3 small_step = float3(1, 0, 0);
	const float step = 1;
	const float2 k = float2(1, -1);

	return normalize(
		k.xyy * map(pos + k.xyy * step) +
		k.yyx * map(pos + k.yyx * step) +
		k.yxy * map(pos + k.yxy * step) +
		k.xxx * map(pos + k.xxx * step)
	);
}

uint wang_hash(inout uint seed) {
    seed = uint(seed ^ uint(61)) ^ uint(seed >> uint(16));
    seed *= uint(9);
    seed = seed ^ (seed >> 4);
    seed *= uint(0x27d4eb2d);
    seed = seed ^ (seed >> 15);
    return seed;
}
 
float randomFloat(inout uint state) {
    return float(wang_hash(state)) / 4294967296.0;
}
 
float3 randomUnitVector(inout uint state) {
    float z = randomFloat(state) * 2.0f - 1.0f;
    float a = randomFloat(state) * 6.28;
    float r = sqrt(1.0f - z * z);
    float x = r * cos(a);
    float y = r * sin(a);
    return float3(x, y, z);
}

struct HitInfo {
	float3 albedo;
	float distance;
	float3 emissive;
	float3 normal;
};

#define MAX_DIST 10000.

// float3 rayMarch(float3 ray_origin, float3 ray_dir) {
void rayMarch(float3 ray_origin, float3 ray_dir, out HitInfo info) {
	float distance_traveled = 0;
	const int NUMBER_OF_STEPS = 300;
	const float MIN_HIT_DISTANCE = 0.001;
	const float MAX_TRACE_DISTANCE = 1000;

	const float3 light_pos = float3(100, 100, 0);
	const float3 light_dir = normalize(0. - light_pos);

	for (int i = 0; i < NUMBER_OF_STEPS; ++i) {
		float3 current_pos = ray_origin + ray_dir * distance_traveled;
		float closest = 0;

		float3 tex_pos = worldToTex(current_pos);
		if (!all(tex_pos >= 0 && tex_pos < tex_size)) {
			float distance = length(current_pos - tex_position) - tex_size.x * 0.5;
			closest = max(abs(distance), 1);
		}
		else {
			closest = map(tex_pos);
		}

		if ((closest) < MIN_HIT_DISTANCE) {
			info.albedo = float3(0.1, 0.2, 1.);
			info.emissive = 0.;
			info.distance = distance_traveled + closest;
			info.normal = calcNormal(tex_pos);
			return;
		}

		if (distance_traveled > MAX_TRACE_DISTANCE) {
			bool is_in_light = dot(light_dir, ray_dir) < 0;
			info.albedo = float3(0.3, 0.7, 1.);
			info.emissive = float3(1, .9, .7) * is_in_light;
			info.distance = MAX_DIST * 2.;
			break;
		}

		distance_traveled += closest;
	}

	bool is_in_light = dot(light_dir, ray_dir) < 0;
	info.albedo = float3(0.3, 0.7, 1.);
	info.emissive = float3(1, .9, .7) * is_in_light;
	info.distance = MAX_DIST;

	//return float3(1, 0, 0);

#if 0

	for (int i = 0; i < NUMBER_OF_STEPS; ++i) {
		float3 current_pos = ray_origin + ray_dir * distance_traveled;

		float closest = 0;

		float3 tex_pos = worldToTex(current_pos);
		if (any(tex_pos > tex_size)) {
			// closest = length(tex_pos) / 10.;
			closest = length(tex_pos) / 10.;
			
			// return float4(1, 0, 0, 1);
		}
		else if (any(tex_pos < 0)) {
			// move a bit closer
			closest = length(tex_pos) / 2.;
		}
		else {
			float4 result = map(tex_pos);
			closest = result.w;
		}

		// hit
		if (closest < MIN_HIT_DISTANCE) {
			//return float3(1, 0, 0);
			// float base_colour = 1 - (float(i) / NUMBER_OF_STEPS);
			//float3 material = lerp(float3(1, 0, 0), float3(0, 0, 1), float(i) / NUMBER_OF_STEPS);
			float3 material = normalize((tex_pos.xyz - 230.0) / 50);
			
			//float ambient_occlusion = 1 - (float)i / (NUMBER_OF_STEPS - 1);
			// material *= base_colour;

			float3 normal = calcNormal(tex_pos);

			const float3 light_pos = float3(sin(time) * 2, -5, cos(time) * 2) * 20.;
			float3 dir_to_light = normalize(current_pos - light_pos);

			float diffuse_intensity = max(0, dot(normal, dir_to_light));

			return material * saturate(diffuse_intensity + float3(0.4, 0.4, 0.4));
		}

		// miss
		if (distance_traveled > MAX_TRACE_DISTANCE) {
			return float3(0, 1, 1);
		}

		distance_traveled += closest;
	}
#endif
	
	//return float3(1.0, 0.5, 0.1);
}

float3 rayTrace(float3 ray_pos, float3 ray_dir, inout uint rng_state) {
	const int MAX_BOUNCES = 10;

	float3 colour = 1;
	float3 light = 0.;

	HitInfo info;

	for (int bounce = 0; bounce < MAX_BOUNCES; ++bounce) {
		rayMarch(ray_pos, ray_dir, info);

		light += info.emissive * colour;
		colour *= info.albedo;

		if (info.distance >= MAX_DIST) {
			if (bounce == 0) {
				return info.albedo;
			}
			
			break;
		}

		ray_pos = ray_pos + ray_dir * info.distance + info.normal;
		ray_dir = normalize(info.normal + randomUnitVector(rng_state));
	}

	return light;
}

float4 main(PixelInput input) : SV_TARGET {
	// convert to range (-1, 1)
	float2 uv = input.uv * 2.0 - 1.0;
	float aspect_ratio = img_width / img_height;
	uv.y *= aspect_ratio;

	uint rng_state = uint(uint(input.uv.x) * uint(1973) + uint(input.uv.y) * uint(9277)) | uint(1);

#if 1
	// float3 ray_dir = normalize(cam_fwd * cam_zoom + cam_right * uv.x + cam_up * uv.y);
	float3 ray_dir = normalize(cam_fwd + cam_right * uv.x + cam_up * uv.y);
	float3 ray_origin = cam_pos + cam_fwd * cam_zoom;
	// float3 colour = rayMarch(cam_pos, ray_dir);
	float3 colour = rayTrace(ray_origin, ray_dir, rng_state);
#else
	float3 ray_ori = float3(0, 0, -100);
	float3 ray_dir = float3(uv, .5);
	float3 colour = rayMarch(ray_ori, normalize(ray_dir));
#endif

	return float4(sqrt(colour), 1.0);
}