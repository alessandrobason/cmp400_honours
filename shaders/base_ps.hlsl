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
	float unused__2;   ///////-PADDING-///////
	// float3 ray_dir;
	// float unused__3;   ///////-PADDING-///////
};

#define SIZE 10.0
#define HSIZE (SIZE/2.0)

Texture3D<float> vol_tex : register(t0);
sampler sampler0;

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

float4 map(float3 pos) {
	// pos /= 10.0;
	// pos *= float3(WIDTH, HEIGHT, DEPTH);
	// int3 coords = pos;
	// return vol_tex[coords];
#if 1
	// float3 coords = ((pos / HSIZE) + 1.0) / 2.0;
	float3 coords = ((pos / SIZE) + 1.0) / 2.0;

	float value = vol_tex.SampleLevel(sampler0, coords, 0);

	return float4(coords, value);
#else
	float displacement = sin(5 * pos.x) * sin(5 * pos.y) * sin(5 * pos.z) * sin(time * 3) * 0.4;
	float sphere_0 = distFromSphere(pos, float3(-1, 0, 0), 2.3);
	float sphere_1 = distFromSphere(pos, float3(1, 0, 0), 2.3);

	// return sminCubic(sphere_0, sphere_1, max(sin(time * 3) + 1 * 0.5, 0.1));
	return sminCubic(sphere_0, sphere_1, 0.);
#endif
	// return sphere_0 + displacement;
}

float3 calcNormal(float3 pos) {
#if 1
	const float3 small_step = float3(0.5, 0, 0);

	float3 gradient = float3(
		map(pos + small_step.xyy).w - map(pos - small_step.xyy).w,
		map(pos + small_step.yxy).w - map(pos - small_step.yxy).w,
		map(pos + small_step.yyx).w - map(pos - small_step.yyx).w
	);

	return normalize(gradient);
#else
	return 0;
#endif
}

float3 rayMarch(float3 ray_origin, float3 ray_dir) {
	float distance_traveled = 0.0;
	const int NUMBER_OF_STEPS = 100;
	const float MIN_HIT_DISTANCE = 0.001;
	const float MAX_TRACE_DISTANCE = 1000;

	for (int i = 0; i < NUMBER_OF_STEPS; ++i) {
		float3 current_pos = ray_origin + ray_dir * distance_traveled;

		float4 result = map(current_pos);
		float closest = result.w;
		// float closest = map(current_pos);
#if 1
		// hit
		if (closest < MIN_HIT_DISTANCE) {
			float3 normal = calcNormal(current_pos);

			const float3 light_pos = float3(2, -5, 3);
			float3 dir_to_light = normalize(current_pos - light_pos);

			float diffuse_intensity = max(0, dot(normal, dir_to_light));

			float3 diffuse = float3(1, 0, 0) * diffuse_intensity;

			return saturate(diffuse);
		}

		// clamp to box size
		if (
			current_pos.x > SIZE || current_pos.y > SIZE || current_pos.z > SIZE ||
			current_pos.x < -SIZE || current_pos.y < -SIZE || current_pos.z < -SIZE
		) {
			// return float3(1, 0, 0);
			break;
		} 
		// miss
		if (distance_traveled > MAX_TRACE_DISTANCE) {
			// return result.rgb;
			break;
		}

		distance_traveled += closest;
#endif
	}
	
	return 0.07;
}

float3x3 setCamera(float3 pos, float3 target, float rot) {
	float3 fwd       = normalize(target-pos);
	float3 up        = float3(sin(rot), cos(rot),0.0);
	float3 right     = normalize( cross(fwd,up) );
	float3 actual_up =          ( cross(right,fwd) );
    return float3x3( right, actual_up, fwd );
}

float4 main(PixelInput input) : SV_TARGET {
	// convert to range (-1, 1)
	float2 uv = input.uv * 2.0 - 1.0;
	// float2 res = float2(img_width, img_height);
	// float2 uv = (input.uv * 2.0 - res) / res.y;

#if 1
	float3x3 camera_mat = float3x3(cam_right, cam_up, cam_fwd);

	float focal_length = 1.5;

	float3 ray_dir = normalize(mul(camera_mat, normalize(float3(uv, focal_length))));

	float3 colour = rayMarch(cam_pos, ray_dir);

#else
	float sint = sin(time) / 2. + .5;

	float3 camera_pos = float3(0, 0, -10 + sint);
	float3 ray_origin = camera_pos;
	float3 ray_dir    = float3(uv, 1);

	float3 colour = rayMarch(camera_pos, ray_dir);

#endif

	return float4(colour, 1.0);
}