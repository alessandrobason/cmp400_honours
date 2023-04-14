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

struct BrushPosData {
	float3 brush_pos;
	float padding__3;
	float3 brush_norm;
	float padding__4;
};

Texture3D<float> vol_tex : register(t0);
StructuredBuffer<BrushPosData> brush_data : register(t1);
sampler sampler_0;

// bad but cheap normal calculation
#define USE_SHITTY_NORMAL 0 

// == scene functions ================================

float3 worldToTex(float3 world) {
	return world - tex_position + tex_size * 0.5;
}

#if USE_SHITTY_NORMAL
float trilinearInterpolation(float3 pos, float3 size, out float3 normal) {
#else
float trilinearInterpolation(float3 pos, float3 size) {
#endif

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

#if USE_SHITTY_NORMAL
	const float2 k = float2(1, -1) * delta.x;

	normal = normalize(
		k.xyy * map_end.x +
		k.yyx * map_start.z +
		k.yxy * map_start.y +
		k.xxx * map_end.w
	);
#endif

#undef map

    const float c0 = c.x * rem.y + c.y * delta.y;
    const float c1 = c.z * rem.y + c.w * delta.y;

    return c0 * rem.z + c1 * delta.z;
}

float preciseMap(float3 coords) {
	//return vol_tex.Load(int4(coords, 0));
	return trilinearInterpolation(coords, tex_size);
}

#if USE_SHITTY_NORMAL
float map(float3 coords, out float3 normal) {
	//return vol_tex.Load(int4(coords, 0));
	return trilinearInterpolation(coords, tex_size, normal);
}
#else
float map(float3 coords) {
	return vol_tex.Load(int4(coords, 0));
	//return trilinearInterpolation(coords, tex_size);
}
#endif

bool isInsideTexture(float3 pos) {
	return all(pos >= 0 && pos < 512);
}

#if USE_SHITTY_NORMAL == 0
float3 calcNormal(float3 pos) {
	// const float step = (cos(time) * .5 + .5) * 100.;
	const float step = 3.;
	const float2 k = float2(1, -1);

	return normalize(
		k.xyy * preciseMap(pos + k.xyy * step) +
		k.yyx * preciseMap(pos + k.yyx * step) +
		k.yxy * preciseMap(pos + k.yxy * step) +
		k.xxx * preciseMap(pos + k.xxx * step)
	);
}
#endif

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

float3 rayMarch(float3 ray_origin, float3 ray_dir) {
	float distance_traveled = 0;
	const int NUMBER_OF_STEPS = 300;
	const float ROUGH_MIN_HIT_DISTANCE = 1;
	const float MIN_HIT_DISTANCE = .005;
	const float MAX_TRACE_DISTANCE = 1000;

	for (int i = 0; i < NUMBER_OF_STEPS; ++i) {
		float3 current_pos = ray_origin + ray_dir * distance_traveled;
		float closest = 0;
		float3 normal = 0;

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
#if USE_SHITTY_NORMAL
			closest = map(tex_pos, normal);
#else
			closest = map(tex_pos);
#endif
		}

		if (closest < ROUGH_MIN_HIT_DISTANCE) {
			closest = preciseMap(tex_pos);

			if (closest < MIN_HIT_DISTANCE) {
	#if USE_SHITTY_NORMAL == 0
				normal = calcNormal(tex_pos);
	#endif
				float3 material = normal * .5 + .5;

				const float3 light_pos = float3(sin(time) * 2, -5, cos(time) * 2) * 20.;
				float3 dir_to_light = normalize(current_pos - light_pos);

				float diffuse_intensity = max(0, dot(normal, dir_to_light));

				return material * saturate(diffuse_intensity + 0.2);
			}
		}

		if (distance_traveled > MAX_TRACE_DISTANCE) {
			break;
		}

		distance_traveled += closest;
	}

	return float3(0.2, 0.5, 1.);

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

	return float3(0.2, 0.5, 1.);
#endif
}

float getMouseDist(float3 pos) {
	const float radius = 18.;
	return length(pos - brush_data[0].brush_pos) - radius;
}

float3 rayMarchWithMouse(float3 ray_origin, float3 ray_dir, inout uint rng_state) {
	float distance_traveled = 0;
	const int NUMBER_OF_STEPS = 300;
	const float ROUGH_MIN_HIT_DISTANCE = 1;
	const float MIN_HIT_DISTANCE = .005;
	const float MAX_TRACE_DISTANCE = 1000;
	const int MAX_BOUNCES = 0;
	float3 current_pos;

	bool is_mouse = false;
	const float3 mouse_colour = float3(.4, .4, .8);

	const float3 top_sky_colour = float3(1, 0.1, 0.1);
	const float3 bottom_sky_colour = float3(0.1, 0.1, 1);

	float3 final_colour = 1;
	const float3 light_pos = float3(sin(time) * 2, -5, cos(time) * 2) * 20.;
	const float3 light_dir = normalize(0 - light_pos);

	int step_count = 0;
	int bounce_count = 0;

	for (; step_count < NUMBER_OF_STEPS; ++step_count) {
		current_pos = ray_origin + ray_dir * distance_traveled;
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

		if (!is_mouse) {
			float mouse_close = getMouseDist(current_pos);
			// mouse brush doesn't have to be perfect, just has to give the idea of where the 
			// new brush will be added so lets use ROUGH_MIN_HIT_DISTANCE
			is_mouse = mouse_close < ROUGH_MIN_HIT_DISTANCE;
			closest = min(mouse_close, closest);
		}

		if (closest < ROUGH_MIN_HIT_DISTANCE) {
			closest = preciseMap(tex_pos);

			if (closest < MIN_HIT_DISTANCE) {
				const float3 normal = calcNormal(tex_pos);
				const float3 material = pow(normal * .5 + .5, 4.) * 0. + 1.;

				float diffuse_intensity = max(0, dot(normal, light_dir));
				final_colour *= material * saturate(diffuse_intensity + 0.2);
				// final_colour *= pow(material, 4);

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
			step_count = NUMBER_OF_STEPS;
			break;
		}

		distance_traveled += closest;
	}

	if (step_count >= NUMBER_OF_STEPS) {
		final_colour *= lerp(top_sky_colour, bottom_sky_colour, ray_dir.y * .5 + .5);
	}

	if (is_mouse) {
		final_colour *= mouse_colour;
	}

	return final_colour;
}

#ifdef RAYTRACING
void rayMarch_trace(float3 ray_origin, float3 ray_dir, out HitInfo info) {
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
			//closest = map(tex_pos);
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
}

float3 rayTrace(float3 ray_pos, float3 ray_dir, inout uint rng_state) {
	const int MAX_BOUNCES = 2;

	float3 colour = 1;
	float3 light = 0.;

	HitInfo info;

	for (int bounce = 0; bounce < MAX_BOUNCES; ++bounce) {
		rayMarch_trace(ray_pos, ray_dir, info);

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
#endif

float4 main(PixelInput input) : SV_TARGET {
	// convert to range (-1, 1)
	float2 uv = input.uv * 2.0 - 1.0;
	float aspect_ratio = img_width / img_height;
	uv.y *= aspect_ratio;

	uint rng_state = uint(uint(input.uv.x * 10000.) * uint(1973) + uint(input.uv.y * 10000.) * uint(9277)) | uint(1);
	rng_state += time * 10000.0;

	float3 ray_dir = normalize(cam_fwd + cam_right * uv.x + cam_up * uv.y);
	float3 ray_origin = cam_pos + cam_fwd * cam_zoom;

	float3 colour = rayMarchWithMouse(ray_origin, ray_dir, rng_state);
	// float3 colour = rayTrace(ray_origin, ray_dir, rng_state);
	
	return float4(sqrt(colour), 1.0);
}