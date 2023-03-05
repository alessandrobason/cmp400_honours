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
};

#define SIZE 10.0
#define HSIZE (SIZE/2.0)

Texture3D<float> vol_tex : register(t0);

// == scene functions ================================

float3 worldToTex(float3 world) {
	const float3 tex_size = float3(1024, 1024, 512);
	
	float3 tex_space = world * 32. + tex_size / 2.;
	tex_space.y = tex_size.y - tex_space.y;
	return tex_space;
}

float4 map(float3 tex_pos) {
	int3 coords = int3(tex_pos);
	int value = vol_tex.Load(int4(coords, 0));
	float distance = (float)(value) / 32.0;

	return float4(coords, distance);
}

bool isInsideTexture(float3 pos) {
	return all(pos >= 0 && pos < float3(1024, 1024, 512));
}

float3 calcNormal(float3 pos) {
	const float3 small_step = float3(1, 0, 0);
	const float step = 64;
	const float2 k = float2(1, -1);

	return normalize(
		k.xyy * map(pos + k.xyy * step).w +
		k.yyx * map(pos + k.yyx * step).w +
		k.yxy * map(pos + k.yxy * step).w +
		k.xxx * map(pos + k.xxx * step).w
	);
}

float3 rayMarch(float3 ray_origin, float3 ray_dir) {
	float distance_traveled = 0.0;
	const int NUMBER_OF_STEPS = 300;
	const float MIN_HIT_DISTANCE = 0.001;
	const float MAX_TRACE_DISTANCE = 1000;

	for (int i = 0; i < NUMBER_OF_STEPS; ++i) {
		float3 current_pos = ray_origin + ray_dir * distance_traveled;

		float3 tex_pos = worldToTex(current_pos);
		if (!isInsideTexture(tex_pos)) {
			// return float(i) / NUMBER_OF_STEPS;
			break;
		}

		float4 result = map(tex_pos);
		float closest = result.w;

		// hit
		if (closest < MIN_HIT_DISTANCE) {
			// float base_colour = 1 - (float(i) / NUMBER_OF_STEPS);
			float3 material = lerp(float3(1, 0, 0), float3(0, 0, 1), float(i) / NUMBER_OF_STEPS);
			// material *= base_colour;

			float3 normal = calcNormal(tex_pos);

			const float3 light_pos = float3(2, -5, 3);
			float3 dir_to_light = normalize(current_pos - light_pos);

			float diffuse_intensity = max(0, dot(normal, dir_to_light));

			float3 diffuse = float3(1, 0, 0) * diffuse_intensity;

			return material * saturate(diffuse);
		}

		// miss
		if (distance_traveled > MAX_TRACE_DISTANCE) {
			return float(i) / NUMBER_OF_STEPS;
			break;
		}

		distance_traveled += closest;
	}
	
	return float3(1.0, 0.5, 0.1);
}

float3x3 setCamera(float3 pos, float3 target, float rot) {
	float3 fwd       = normalize(target-pos);
	float3 up        = float3(sin(rot), cos(rot),0.0);
	float3 right     = normalize( cross(fwd,up) );
	float3 actual_up =          ( cross(right,fwd) );
    return float3x3( right, actual_up, fwd );
}

float get(int3 coords) {
	return vol_tex.Load(int4(coords, 0));
}

float4 main(PixelInput input) : SV_TARGET {
	// convert to range (-1, 1)
	float2 uv = input.uv * 2.0 - 1.0;

#if 1
	float3x3 camera_mat = float3x3(cam_right, cam_up, cam_fwd);
	float focal_length = .5;
	float3 ray_dir = normalize(mul(camera_mat, normalize(float3(uv * 2, focal_length))));
	float3 colour = rayMarch(cam_pos + float3(0, 0, -1.0), ray_dir);
#else
	float3 ray_ori = float3(0, 0, -5);
	float3 ray_dir = float3(uv, 1);
	float3 colour = rayMarch(ray_ori, ray_dir);
#endif

	return float4(colour, 1.0);
}