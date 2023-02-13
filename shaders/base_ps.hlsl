struct PixelInput {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORDS0;
};

cbuffer ShaderData : register(b0) {
	float time;
	float3 __unused0;
};

#define WIDTH  64
#define HEIGHT 64
#define DEPTH  32
#define PRECISION 1

Texture3D<float> vol_tex : register(t0);

// == shape functions ================================

float distFromSphere(float3 pos, float3 centre, float radius) {
	return length(pos - centre) - radius;
}

// == operation functions ============================

// polynomial smooth min
float smin(float a, float b, float k) {
	float h = max(k - abs(a - b), 0.0) / k;
	return min(a, b) - h * h * k * (1.0 / 4.0);
}

// polynomial smooth min
float sminCubic(float a, float b, float k) {
	float h = max(k - abs(a - b), 0.0) / k;
	return min(a, b) - h * h * h * k * (1.0 / 6.0);
}

// == scene functions ================================

float map(float3 pos) {
	// pos /= 10.0;
	// pos *= float3(WIDTH, HEIGHT, DEPTH);
	// int3 coords = pos;
	// return vol_tex[coords];

	float displacement = sin(5 * pos.x) * sin(5 * pos.y) * sin(5 * pos.z) * sin(time * 3) * 0.4;
	float sphere_0 = distFromSphere(pos, float3(-1, 0, 0), 1.5);
	float sphere_1 = distFromSphere(pos, float3(1, 0, 0), 1.5);

	// return sminCubic(sphere_0, sphere_1, max(sin(time * 3) + 1 * 0.5, 0.1));
	return sminCubic(sphere_0, sphere_1, 0.5);
	// return sphere_0 + displacement;
}

float3 calcNormal(float3 pos) {
	const float3 small_step = float3(0.001, 0, 0);

	float3 gradient = float3(
		map(pos + small_step.xyy) - map(pos - small_step.xyy),
		map(pos + small_step.yxy) - map(pos - small_step.yxy),
		map(pos + small_step.yyx) - map(pos - small_step.yyx)
	);

	return normalize(gradient);
}

float3 rayMarch(float3 ray_origin, float3 ray_dir) {
	float distance_traveled = 0.0;
	const int NUMBER_OF_STEPS = 10;
	const float MIN_HIT_DISTANCE = 0.001;
	const float MAX_TRACE_DISTANCE = 10.0;

	for (int i = 0; i < NUMBER_OF_STEPS; ++i) {
		float3 current_pos = ray_origin + ray_dir * distance_traveled;

		float closest = map(current_pos);
	
		// hit
		if (closest < MIN_HIT_DISTANCE) {
			float3 normal = calcNormal(current_pos);

			const float3 light_pos = float3(2, -5, 3);
			float3 dir_to_light = normalize(current_pos - light_pos);

			float diffuse_intensity = max(0, dot(normal, dir_to_light));

			float3 diffuse = float3(1, 0, 0) * diffuse_intensity;

			return saturate(diffuse);
		}

		// miss
		if (distance_traveled > MAX_TRACE_DISTANCE) {
			break;
		}

		distance_traveled += closest;
	}
	
	return 0.07;
}

float4 main(PixelInput input) : SV_TARGET {
	// convert to range (-1, 1)
	float2 uv = input.uv * 2.0 - 1.0;

	float3 camera_pos = float3(0, 0, -5 + sin(time * 2));
	float3 ray_origin = camera_pos;
	float3 ray_dir    = float3(uv, 1);

	float3 colour = rayMarch(ray_origin, ray_dir);

	return float4(colour, 1.0);
}