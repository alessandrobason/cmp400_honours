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
	float padding__0;
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
			return float3(0, 0, 1);
			// break;
		}

		if (any(tex_pos < 0)) {
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

			return material * 1;//saturate(diffuse_intensity + float3(0.01, 0.1, 0.4));
		}

		// miss
		if (distance_traveled > MAX_TRACE_DISTANCE) {
			//return float(i) / NUMBER_OF_STEPS;
			return float3(0, 0, 1);
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

#ifndef QUATERNION
#ifndef PI
#define PI 3.14159265359f
#endif 

#define quat float4
#define quatId float4(0, 0, 0, 1)
#define v3 float3
#define norm normalize
#define FWD v3(0, 0, -1)
#define UP v3(0, 1, 0)
#define RIGHT v3(1, 0, 0)

float4 qmul(float4 q1, float4 q2) {
    return float4(
        q2.xyz * q1.w + q1.xyz * q2.w + cross(q1.xyz, q2.xyz),
        q1.w * q2.w - dot(q1.xyz, q2.xyz)
    );
}

// Vector rotation with a quaternion
// http://mathworld.wolfram.com/Quaternion.html
float3 rotateVec(quat r, float3 v) {
    float4 r_c = r * float4(-1, -1, -1, 1);
    return qmul(r, qmul(float4(v, 0), r_c)).xyz;
}

// A given angle of rotation about a given axis
float4 fromAxisAngle(float3 axis, float angle) {
    float sn = sin(angle * 0.5);
    float cs = cos(angle * 0.5);
    return float4(axis * sn, cs);
}

#endif

float4 main(PixelInput input) : SV_TARGET {
	// convert to range (-1, 1)
	float2 uv = input.uv * 2.0 - 1.0;
	float aspect_ratio = img_width / img_height;
	uv.y *= aspect_ratio;

	const float3 target = 0;
	float2 angle = float2(cos(time * 3.5), sin(time * 2.12));
	angle.y = clamp(angle.y, -PI/4., PI/4.);

	quat rot_x = fromAxisAngle(float3(0, 1, 0), angle.x);
	quat rot_y = fromAxisAngle(float3(1, 0, 0), angle.y);

	float3 f1 = rotateVec(rot_x, FWD);
	float3 f2 = rotateVec(rot_y, f1);

	f1 = norm(f1);
	f2 = norm(f2);

	float3 fwd = norm(f1);

	// quat rot = qmul(rot_y, rot_x);
	// float3 forward = rotateVec(rot, FWD);
	// float3 fwd = norm(forward);

	float3 right, up;

	if (true) {
		right = norm(cross(float3(0, -1, 0), fwd));
		up = norm(cross(right, fwd));
	}
	else {
		right = norm(cross(fwd, float3(0, 1, 0)));
		up = norm(cross(right, fwd));
	}
	// float3 up = norm(rotateVec(rot, float3(0, 1, 0)));
	// float3 right = norm(rotateVec(rot, float3(1, 0, 0)));

	float3 eye = target - fwd * 200.0;

	//float2 angle = float2(cos(time * 3.5), sin(time * 2.12));
	//float deg90 = PI/4.;
	//angle.y = clamp(angle.y, -deg90, deg90);
	//quat rot_x = fromAxisAngle(UP, angle.x);
	//quat rot_y = fromAxisAngle(RIGHT, angle.y);
	//quat rot = qmul(rot_x, rot_y);
	//v3 forward = rotateVec(rot, FWD);

#if 1
	//const float3 target = 0;
	//////float3 eye = float3(sin(time), 0., cos(time));
	////float3 eye = float3(0, sin(time), cos(time));
	////eye = normalize(eye) * 200.0;
	////float3 fwd = normalize(target - eye);
	//v3 fwd = norm(forward);
	//v3 eye = target - fwd * 200.0;
	//float3 right, up;

	//right = norm(rotateVec(rot, RIGHT));
	//up = norm(rotateVec(rot, UP));

	//if (false) {
	//	if (eye.z > 0) {
	//		right = normalize(cross(fwd, float3(0, 1, 0)));
	//	}
	//	else {
	//		right = normalize(cross(float3(0, 1, 0), fwd));
	//	}
	//
	//	up = normalize(cross(right, fwd));
	//}
	//else {
	//	right = normalize(cross(fwd, float3(0, 1, 0)));
	//	up = normalize(cross(right, fwd));
	//}
	float3x3 camera_mat = float3x3(right, up, fwd);
	float focal_length = 2.;
	float3 ray_dir = normalize(mul(camera_mat, normalize(float3(uv * 2, focal_length))));
	float3 colour = rayMarch(eye, ray_dir);
#else
#if 1
	float3x3 camera_mat = float3x3(cam_right, cam_up, cam_fwd);
	float focal_length = 2.;
	float3 ray_dir = normalize(mul(camera_mat, normalize(float3(uv * 2, focal_length))));
	float3 colour = rayMarch(cam_pos, ray_dir);
#else
	float3 ray_ori = float3(0, 0, -100);
	float3 ray_dir = float3(uv, .5);
	float3 colour = rayMarch(ray_ori, normalize(ray_dir));
#endif
#endif


	return float4(colour, 1.0);
}