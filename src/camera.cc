#include "camera.h"

#include "maths.h"
#include "input.h"
#include "system.h"
#include "options.h"

constexpr float y_max_angle = 90.f;
constexpr float radius = 270.f;

static bool isMouseInsideRTV() {
	return gfx::isMainRTVActive() &&
		   gfx::getMainRTVBounds().contains(getMousePos());
}

Camera::Camera() {
	updateVectors();
}

void Camera::update() {
	const Options &options = Options::get();
	static bool is_dragging = false;

	// Zoom stuff

	if (float wheel = getMouseWheel()) {
		zoom_exp += wheel * win::dt * options.zoom_sensitivity;
	}

	float kb_zoom = (float)(isActionDown(Action::ZoomIn) - isActionDown(Action::ZoomOut));
	zoom_exp += kb_zoom * 0.1f * win::dt * options.zoom_sensitivity;

	if (isActionPressed(Action::ResetZoom)) {
		zoom_exp = 1.f;
	}

	// keyboard input in case the user has no mouse
	const vec2 key_rel = vec2(
		(float)(isActionDown(Action::RotateCameraHorPos) - isActionDown(Action::RotateCameraHorNeg)),
		(float)(isActionDown(Action::RotateCameraVerPos) - isActionDown(Action::RotateCameraVerNeg))
	) * 2.f;
	bool any_kb_input = any(key_rel != 0);

	vec2 look_offset = key_rel;

	if (!any_kb_input) {
		// Input stuff to check that the user has started the rotation with the mouse inside the render target

		if (!isMouseDown(MOUSE_RIGHT)) {
			is_dragging = false;
			return;
		}

		if (isMousePressed(MOUSE_RIGHT)) {
			is_dragging = isMouseInsideRTV();
		}

		if (!is_dragging) {
			return;
		}

		look_offset = getMousePosRel();
	}

	// Calculate how much to rotate

	const vec2 offset = look_offset * win::dt * options.look_sensitivity;
	angle.x -= offset.x;
	angle.y += offset.y;
	angle.y = math::clamp(angle.y, -y_max_angle, y_max_angle);

	updateVectors();
}

void Camera::updateVectors() {
	// use polar coordinates to calculate camera position
	const float azimuth_angle = angle.x;
	const float elevation_angle = angle.y;
	const float arad = math::torad(azimuth_angle);
	const float erad = math::torad(elevation_angle);

	constexpr vec3 aim = 0.f;
	constexpr vec3 world_up = vec3(0, 1, 0);

	pos   = vec3(cosf(arad), sinf(erad), sinf(arad)).normalised() * radius;
	fwd   = norm(aim - pos);
	right = norm(cross(world_up, fwd));
	up    = norm(cross(fwd, right));
}

float Camera::getZoom() const {
	return zoom_exp;
}

// adapted from fragment shader
vec3 Camera::getMouseDir() const {
	const vec4 &bounds = gfx::getMainRTVBounds();
	const vec2 &mouse_pos = getMousePos();
	const vec2 &win_size = gfx::main_rtv.size;
	const float aspect_ratio = win_size.x / win_size.y;

	const vec2 rel_pos = mouse_pos - bounds.pos;
	const vec2 norm_pos = rel_pos / bounds.size;
	vec2 uv = norm_pos * 2.f - 1.f;
	uv.y /= -aspect_ratio;

	return norm(fwd + right * uv.x + up * uv.y);
}

bool Camera::shouldSculpt() const {
	return isMousePressed(MOUSE_LEFT) && isMouseInsideRTV();
}

#if 0
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

mat4 rotate(const mat4& mat, float angle, const vec3& v) {
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

mat4 lookAt(const vec3& eye, const vec3& centre, const vec3& up) {
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

#define V2FMT "(%.3f %.3f)"
#define VFMT  "(%.3f %.3f %.3f)"
#define V4FMT "(%.3f %.3f %.3f %.3f)"
#define V2(v) (v).x, (v).y
#define V(v)  (v).x, (v).y, (v).z
#define V4(v) (v).x, (v).y, (v).z, (v).w

/*
https://github.com/MartinWeigel/Quaternion/blob/master/Quaternion.c
http://courses.cms.caltech.edu/cs171/assignments/hw3/hw3-notes/notes-hw3.html

https://oguz81.github.io/ArcballCamera/
*/

struct quat : vec4 {
	using vec4::vec4T;

	quat(const vec4& v) {
		x = v.x;
		y = v.y;
		z = v.z;
		w = v.w;
	}

	static const quat identity;

	static quat fromAxisAngle(const vec3& axis, float angle) {
		float s = sinf(angle / 2.f);
		return quat(axis * s, cosf(angle / 2.f));
	}

	static quat fromEuler(float roll, float pitch, float yaw) {
		float cr = cosf(roll * 0.5f);
		float sr = sinf(roll * 0.5f);
		float cp = cosf(pitch * 0.5f);
		float sp = sinf(pitch * 0.5f);
		float cy = cosf(yaw * 0.5f);
		float sy = sinf(yaw * 0.5f);

		quat q;
		q.w = cr * cp * cy + sr * sp * sy;
		q.x = sr * cp * cy - cr * sp * sy;
		q.y = cr * sp * cy + sr * cp * sy;
		q.z = cr * cp * sy - sr * sp * cy;

		return q;
	}

	mat4 toMat() const {
		mat4 out;

		const float xx2 = x * x * 2.f;
		const float xy = x * y;
		const float xz = x * z;
		const float xw = x * w;

		const float yy2 = y * y * 2.f;
		const float yz = y * z;
		const float yw = x * w;

		const float zz2 = z * z * 2.f;
		const float zw = x * w;

		out[0][0] = 1.f - yy2 - zz2;
		out[0][1] = 2.f * (xy - zw);
		out[0][2] = 2.f * (xz + yw);

		out[1][0] = 2.f * (xy + zw);
		out[1][1] = 1.f - xx2 - zz2;
		out[1][2] = 2.f * (yz - xw);

		out[2][0] = 2.f * (xz - yw);
		out[2][1] = 2.f * (yz + xw);
		out[2][2] = 1.f - xx2 - yy2;

		out[3][3] = 1;

		return out;
	}

	quat operator*(const quat& q) const {
		vec3 v1 = v;
		vec3 v2 = q.v;
		float s1 = s;
		float s2 = q.s;

		vec3 v_out = cross(v1, v2) + v2 * s1 + v1 * s2;
		float s_out = s1 * s2 - vec3::dot(v1, v2);

		return quat(v_out, s_out);
	}

	quat conjugate() const {
		return { -x, -y, -z, w };
	}

	quat inverse() const {
		quat q1 = conjugate();
		return q1 / normalised();
	}

	vec3 rotate(const vec3& p) const {
		float xx = x * x;
		float xy = x * y;
		float xz = x * z;
		float xw = x * w;

		float yy = y * y;
		float yz = y * z;
		float yw = y * w;

		float zz = z * z;
		float zw = z * w;

		float ww = w * w;

		// Formula from http://www.euclideanspace.com/maths/algebra/realNormedAlgebra/quaternions/transforms/index.htm
		// p2.x = w*w*p1.x + 2*y*w*p1.z - 2*z*w*p1.y + x*x*p1.x + 2*y*x*p1.y + 2*z*x*p1.z - z*z*p1.x - y*y*p1.x;
		// p2.y = 2*x*y*p1.x + y*y*p1.y + 2*z*y*p1.z + 2*w*z*p1.x - z*z*p1.y + w*w*p1.y - 2*x*w*p1.z - x*x*p1.y;
		// p2.z = 2*x*z*p1.x + 2*y*z*p1.y + z*z*p1.z - 2*w*y*p1.x - y*y*p1.z + 2*w*x*p1.y - x*x*p1.z + w*w*p1.z;

		vec3 out;

		out.x = ww * p.x + 2.f * yw * p.z - 2.f * zw * p.y + xx * p.x + 2.f * xy * p.y + 2.f * xz * p.z - zz * p.x - yy * p.x;
		out.y = 2.f * xy * p.x + yy * p.y + 2.f * yz * p.z + 2.f * zw * p.x - zz * p.y + ww * p.y - 2.f * xw * p.z - xx * p.y;
		out.z = 2.f * xz * p.x + 2.f * yz * p.y + zz * p.z - 2.f * yw * p.x - yy * p.z + 2.f * xw * p.y - xx * p.z + ww * p.z;

		return out;
	}
};

const quat quat::identity = quat(0, 0, 0, 1);
#endif