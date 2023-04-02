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

float4 map(float3 coords) {
	float distance = vol_tex.Load(int4(coords, 0));
	return float4(coords, distance);
}

bool isInsideTexture(float3 pos) {
	return all(pos >= 0 && pos < 512);
}

float3 calcNormal(float3 pos) {
	const float3 small_step = float3(1, 0, 0);
	const float step = 1;
	const float2 k = float2(1, -1);

	return normalize(
		k.xyy * map(pos + k.xyy * step).w +
		k.yyx * map(pos + k.yyx * step).w +
		k.yxy * map(pos + k.yxy * step).w +
		k.xxx * map(pos + k.xxx * step).w
	);
}

float3 rayMarch(float3 ray_origin, float3 ray_dir) {
	float distance_traveled = 0;
	const int NUMBER_OF_STEPS = 300;
	const float MIN_HIT_DISTANCE = 0.001;
	const float MAX_TRACE_DISTANCE = 1000;

	for (int i = 0; i < NUMBER_OF_STEPS; ++i) {
		float3 current_pos = ray_origin + ray_dir * distance_traveled;

		float closest = 0;

		float3 tex_pos = worldToTex(current_pos);
		if (any(tex_pos > tex_size)) {
			// closest = length(tex_pos) / 2.;
			closest = length(tex_pos) / 10.;
			//return float3(1, 0, 1);
			// break;
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
			//return float(i) / NUMBER_OF_STEPS;
			return float3(0, 1, 1);
		}

		distance_traveled += closest;
	}
	
	return float3(1.0, 0.5, 0.1);
}

float4 main(PixelInput input) : SV_TARGET {
	// convert to range (-1, 1)
	float2 uv = input.uv * 2.0 - 1.0;
	float aspect_ratio = img_width / img_height;
	uv.y *= aspect_ratio;

#if 1
	float3 ray_dir = normalize(cam_fwd * cam_zoom + cam_right * uv.x + cam_up * uv.y);
	float3 colour = rayMarch(cam_pos, ray_dir);
#else
	float3 ray_ori = float3(0, 0, -100);
	float3 ray_dir = float3(uv, .5);
	float3 colour = rayMarch(ray_ori, normalize(ray_dir));
#endif

	return float4(colour, 1.0);
}