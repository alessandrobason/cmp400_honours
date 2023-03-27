#include "camera.h"

#include "maths.h"
#include "input.h"
#include "system.h"

struct mat4 {
	union {
		float data[16];
		float m[4][4];
		vec4 rows[4];
	};
	vec4& operator[](size_t i) { return rows[i]; }
	const vec4& operator[](size_t i) const { return rows[i]; }

	vec4 operator*(const vec4& v) const {
		return vec4(
			m[0][0] * v[0] + m[0][1] * v[1] + m[0][2] * v[2] + m[0][3] * v[3],
			m[1][0] * v[0] + m[1][1] * v[1] + m[1][2] * v[2] + m[1][3] * v[3],
			m[2][0] * v[0] + m[2][1] * v[1] + m[2][2] * v[2] + m[2][3] * v[3],
			m[3][0] * v[0] + m[3][1] * v[1] + m[3][2] * v[2] + m[3][3] * v[3]
		);
	}

	vec3 operator*(const vec3& v) const {
		return vec3(
			m[0][0] * v[0] + m[0][1] * v[1] + m[0][2] * v[2] + m[0][3] * 1.f,
			m[1][0] * v[0] + m[1][1] * v[1] + m[1][2] * v[2] + m[1][3] * 1.f,
			m[2][0] * v[0] + m[2][1] * v[1] + m[2][2] * v[2] + m[2][3] * 1.f
		);
	}

	static const mat4 identity;
};

const mat4 mat4::identity = { 
	1, 0, 0, 0, 
	0, 1, 0, 0, 
	0, 0, 1, 0, 
	0, 0, 0, 1, 
};

mat4 rotate(const mat4 &mat, float angle, const vec3 &v) {
	const float c = cosf(angle);
	const float s = sinf(angle);

	const vec3 axis = norm(v);
	const vec3 t = axis * (1.f - c);

	mat4 rotate;
	rotate[0][0] = c + t.x * axis.x;
	rotate[0][1] = t.x * axis.y + s * axis.z;
	rotate[0][2] = t.x * axis.z - s * axis.y;

	rotate[1][0] = t.y * axis.x - s * axis.z;
	rotate[1][1] = c + t.y * axis.y;
	rotate[1][2] = t.y * axis.z + s * axis.x;

	rotate[2][0] = t.z * axis.x + s * axis.y;
	rotate[2][1] = t.z * axis.y - s * axis.x;
	rotate[2][2] = c + t.z * axis.z;

	mat4 result;
	result[0] = mat[0] * rotate[0][0] + mat[1] * rotate[0][1] + mat[2] * rotate[0][2];
	result[1] = mat[0] * rotate[1][0] + mat[1] * rotate[1][1] + mat[2] * rotate[1][2];
	result[2] = mat[0] * rotate[2][0] + mat[1] * rotate[2][1] + mat[2] * rotate[2][2];
	result[3] = mat[3];
	return result;
}

mat4 lookAt(const vec3 &eye, const vec3 &centre, const vec3 &up) {
	// forward, right, and up
	const vec3 f = norm(centre - eye);
	const vec3 r = norm(cross(f, up));
	const vec3 u = cross(r, f);

	mat4 result = mat4::identity;
	result[0][0] = r.x;
	result[1][0] = r.y;
	result[2][0] = r.z;
	result[0][1] = u.x;
	result[1][1] = u.y;
	result[2][1] = u.z;
#if 1 // right handed
	result[0][2] = -f.x;
	result[1][2] = -f.y;
	result[2][2] = -f.z;
	result[3][0] = -dot(r, eye);
	result[3][1] = -dot(u, eye);
	result[3][2] = dot(f, eye);
#else // left handed
	result[0][2] = f.x;
	result[1][2] = f.y;
	result[2][2] = f.z;
	result[3][0] = -dot(r, eye);
	result[3][1] = -dot(u, eye);
	result[3][2] = -dot(f, eye);
#endif
	return result;
}

void Camera::update() {
#if 0
	constexpr float sensitivity = 15.1f;
	static float yaw = 90.f, pitch = 0.f, zoom = 1.f;

	float wheel = getMouseWheel();
	if (wheel) {
		zoom += wheel * win::dt * 50.f * copysignf(1.f, -fwd.z);
		pos = vec3(0) - fwd * 100.0f * zoom;
	}

	if (isMouseDown(MOUSE_RIGHT)) {
		vec2 offset = (vec2)getMousePosRel() * sensitivity * win::dt;
		yaw += offset.x;
		pitch += offset.y;
		if (pitch > 89.0f) {
			pitch = 89.0f;
		}
		if (pitch < -89.0f) {
			pitch = -89.0f;
		}
		float yr = math::torad(yaw);
		float pr = math::torad(pitch);
		vec3 front = {
			cosf(yr) * cosf(pr),
			sinf(pr),
			sinf(yr) * cosf(pr)
		};
		
		fwd = norm(front);
		right = norm(cross(up, fwd));
		up = norm(cross(fwd, right));
		pos = vec3(0) - fwd * 100.0f * zoom;
		info("pos: %.3f %.3f %.3f, fwd: %.3f %.3f %.3f", pos.x, pos.y, pos.z, fwd.x, fwd.y, fwd.z);

		target = fwd + pos;
	}
#endif

	if (!isMouseDown(MOUSE_RIGHT)) {
		return;
	}

	const vec2i mouse_delta = getMousePosRel();
	if (all(mouse_delta == 0)) {
		return;
	}

	// get rotation angle
	const vec2i view_size = gfx::main_rtv.size;
	const vec2 delta_angle = vec2(math::pi) / (vec2)view_size;
	vec2 angle = (vec2)mouse_delta * delta_angle;

	fwd = norm(target - pos);
	right = norm(cross(fwd, up));

	const mat4 rot_mat_x = rotate(mat4::identity, angle.x, vec3(0, 1, 0));
	const mat4 rot_mat_y = rotate(mat4::identity, angle.y, right);

	pos = (rot_mat_x * (pos - target)) + target;
	vec3 final_pos = (rot_mat_y * (pos - target)) + target;

	updateView(final_pos, target, up);

#if 0
	float wheel = getMouseWheel();
	if (wheel) {
		constexpr float zoom_spd = 500.f;
		pos += norm(target - pos) * wheel * win::dt * zoom_spd;
	}

	if (!isMouseDown(MOUSE_RIGHT)) {
		return;
	}

	// get rotation angle
	const vec2i view_size = gfx::main_rtv.size;
	const vec2 delta_angle = vec2(math::pi * 2.f, math::pi) / (vec2)view_size;
	vec2 angle = (vec2)getMousePosRel() * delta_angle;

	const vec3 fwd = norm(target - pos);
	const vec3 right = cross(fwd, up);

	// if the camera is exactly up
	const float cos_angle = dot(fwd, up);
	if (cos_angle * copysignf(1.f, angle.y) > 0.99f) {
		angle.y = 0.f;
	}

	const mat4 rot_mat_x = rotate(mat4::identity, angle.x, vec3(0, 1, 0));
	const mat4 rot_mat_y = rotate(mat4::identity, angle.y, right);

	pos = (rot_mat_x * (pos - target)) + target;
	const vec3 final_pos = (rot_mat_y * (pos - target)) + target;

	updateView(final_pos, target, vec3(0, 1, 0));
#endif
}

void Camera::updateView(const vec3& eye, const vec3& lookat, const vec3& new_up) {
	pos = eye;
	target = lookat;
	up = new_up;
	//mat4 view_mat = lookAt(pos, target, up);
	//view = view_mat.data;

	fwd = norm(target - pos);
	right = norm(cross(fwd, up));
}