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
		return q1 / normalise();
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

static vec2 mouse_start = 0;
static vec2 mouse_cur = 0;
static bool dragging = false;

static quat last_rot = quat::identity;
static quat cur_rot = quat::identity;

static quat computeRotation(vec2 cur_pos, vec2 start_pos) {
	// convert the 2D start/cur positions to 3D vectors inside unit sphere
	vec3 start = vec3(start_pos, 0.f);
	float length2 = start.mag2();

	if (length2 > 1.f) {
		start /= sqrtf(length2);
	}
	// if length < 1, calculate z using phytagora
	else {
		start.z = sqrtf(1.f - length2);
	}

	vec3 end = vec3(cur_pos, 0.f);
	length2 = end.mag2();

	if (length2 > 1.f) {
		end /= sqrtf(length2);
	}
	else {
		end.z = sqrtf(1.f - length2);
	}

	vec3 axis = norm(cross(start, end));

	float angle = acosf(math::clamp(dot(start, end), -1.f, 1.f));

	return quat::fromAxisAngle(axis, angle);
}

vec3 getArcVec(vec2 pos) {
	float radius_squared = 1.f;

	vec3 pt = vec3(pos - (vec2)win::getSize() / 2., 0.f);

	float mag2 = pt.mag2();

	if (mag2 < radius_squared) {
		pt.z = sqrtf(radius_squared - mag2);
	}
	else {
		pt.z = 0.f;
	}

	pt.z *= -1.f;
	return pt;
}

void Camera::update() {
	return;
	static vec2 angle = 0;
	static constexpr float sensitivity = 5.f;
	static bool dragging = false;
	static vec2 start = 0;
	static vec2 end = 0;

	if (!dragging) {
		if (isMouseDown(MOUSE_LEFT)) {
			dragging = true;
			start = getMousePos();
		}
		else {
			return;
		}
	}
	
	if (!isMouseDown(MOUSE_LEFT)) {
		dragging = false;
		return;
	}

	end = getMousePos();

	vec3 start_pt = getArcVec(start);
	vec3 end_pt = getArcVec(end);

	info("s " VFMT " e " VFMT, V(start_pt), V(end_pt));

	quat act_rot = quat::identity;
	vec3 axis = norm(cross(end_pt, start_pt));

	if (axis.mag2() > 0.001f) {
		float angle_cos = dot(end_pt, start_pt);
		act_rot = quat(axis, angle_cos);
	}

	info("q " V4FMT, V4(act_rot));

	fwd = norm(act_rot.rotate(fwd));
	pos = target - fwd * 200.f;
	right = norm(cross(fwd, vec3(0, 1, 0)));
	up = norm(cross(right, fwd));

#if 0

	const vec2i mouse_delta = -getMousePosRel();
	if (all(mouse_delta == 0)) {
		return;
	}

	// get rotation angle
	const vec2i view_size = gfx::main_rtv.size;
	const vec2 delta_angle = vec2(math::pi) / (vec2)view_size;
	angle += (vec2)mouse_delta * delta_angle * sensitivity;

	// Keep the Y angle in the range [-360, 360]
	//if (angle.y >= math::torad(270.f))  angle.y -= math::pi2;
	//if (angle.y <= math::torad(-270.f)) angle.y += math::pi2;
	angle.y = math::clamp(angle.y, math::torad(-89.f), math::torad(89.f));

	//fwd = norm(target - pos);
	//right = norm(cross(fwd, vec3(0, 1, 0)));
	const vec3 r2 = vec3(right.x, right.y, -right.z);

#if 0
	quat rot_x = quat::identity, rot_y = quat::identity;

	rot_x = quat::fromAxisAngle(vec3(0, 1, 0), angle.x);

	if (false) {
		//rot_x = quat::fromAxisAngle(vec3(0, 1, 0), angle.x);
	}
	else {
		rot_y = quat::fromAxisAngle(vec3(1, 0, 0), angle.y);
	}
#endif

	quat rot_x = quat::fromAxisAngle(vec3(0, 1, 0), angle.x);
	quat rot_y = quat::fromAxisAngle(vec3(1, 0, 0), angle.y);
	vec3 forward = (rot_x * rot_y).rotate(vec3(0, 0, -1));
	//vec3 axis = forward * vec3(angle.y, angle.x, 0.f);
	//quat rot = quat::fromAxisAngle(axis.normalise(), axis.mag());

	fwd = norm(forward);
	pos = target - fwd * 200.f;
	info("%.3f", pos.mag());
	//pos = rot.rotate(vec3(0, 0, 200));
	//fwd = norm(target - pos);
	right = norm(cross(fwd, vec3(0, 1, 0)));
	up = norm(cross(right, fwd));

	info("f " VFMT, V(fwd));
#endif
#if 0
	return;

	//const quat rot_x = quat::fromAxisAngle(up, angle.x);
	//const quat rot_x = quat::identity;
	//const quat rot_y = quat::fromAxisAngle(right, angle.y);
	//const quat rot_y = quat::identity;

	const quat rot = rot_y * rot_x;

	pos = rot.rotate(vec3(0, 0, 200));
	fwd = norm(target - pos);

	//info("angle.y: %.2frad %.2fdeg, right: " VFMT, angle.y, math::todeg(angle.y), V(right));
	info("p " VFMT ", f " VFMT ", r " VFMT ", u " VFMT, V(pos), V(fwd), V(right), V(up));
	
	right = norm(cross(fwd, vec3(0, 1, 0)));

	if (angle.y > math::torad(-90.f) && angle.y < math::torad(90.f)) {
		//right = -right;
		//	right = norm(cross(fwd, vec3(0, 1, 0)));
	}
	else {
		//right = right;
		//	right = norm(cross(vec3(0, 1, 0), fwd));
	}

	//up = norm(cross(fwd, right));
	up = norm(cross(right, fwd));
#endif
	
	//right = norm(rot.rotate(fwd.z > 0 ? vec3(1, 0, 0) : vec3(-1, 0, 0)));
	//up = norm(rot.rotate(vec3(0, 1, 0)));

	// fwd2 = norm(rot_x.rotate(fwd2));
	// fwd.y = fwd2.x;
	// fwd.x = 0.f;
	// fwd = norm(vec3(0, fwd2.x, fwd2.z));
	// pos = vec3(0) - fwd * 200.f;
	// right = norm(cross(fwd, vec3(0, 1, 0)));
	// up = norm(cross(right, fwd));

	//info("pos: " VFMT ", fwd: " VFMT ", right: " VFMT, V(pos), V(fwd), V(right));


	//if (!dragging) {
	//	if (isMouseDown(MOUSE_LEFT)) {
	//		dragging = true;
	//		mouse_start = getMousePos();
	//	}
	//	else {
	//		return;
	//	}
	//}
	//
	//if (!isMouseDown(MOUSE_LEFT)) {
	//	dragging = false;
	//	last_rot = cur_rot;
	//	cur_rot = quat::identity;
	//	return;
	//}

	//mouse_cur = getMousePos();

	//cur_rot = computeRotation(mouse_cur, mouse_start);

	////quat rotation = cur_rot * last_rot;
	//fwd = norm(rotation.rotate(fwd));
	//pos = vec3(0) - fwd * 200.f;
	//right = norm(cross(fwd, vec3(0, 1, 0)));
	//up = norm(cross(right, fwd));

	// mat4 current_rotation = compute_rot_mat(mouse_cur, mouse_start);

	// getCurrentRot() = current_rotation * last_rotation;

#if 0
	static float pitch = math::torad(0.f),
		yaw = math::torad(90.f);

	if (isMouseDown(MOUSE_RIGHT) && false) {
		constexpr float sensitivity = 1.f;
		vec2 offset = (vec2)getMousePosRel() * sensitivity * win::dt;
		yaw += offset.x;
		pitch += offset.y;
	}

	static bool xx = true;

	if (xx && false) yaw += win::dt;
	else pitch += win::dt;

	xx = !xx;

	//pitch += win::dt;
	//if (pitch >= math::torad(360.f)) pitch -= math::torad(360.f);
	//info("pitch: %.3frad %.3fdeg", pitch, math::todeg(pitch));
	//pitch = math::clamp(pitch, math::torad(90.f), math::torad(269.f));

	float cp = cosf(pitch);

	vec3 dir = vec3(
		cosf(yaw) * cp,
		sinf(pitch),
		sinf(yaw) * cp
	).normalise();

	dir.z = -fabsf(dir.z);

	info("dir: " VFMT, V(dir));

	fwd = norm(dir);
	pos = target - fwd * 200.f;
	// target = pos + fwd;
	right = norm(cross(fwd, vec3(0, 1, 0)));
	up = norm(cross(right, fwd));
	//mat4 view = lookAt(pos, 0, vec3(0, 1, 0));
#endif

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
		pos = (vec3(0) - fwd) * 200.0f * zoom;
		info("pos: %.3f %.3f %.3f, fwd: %.3f %.3f %.3f", pos.x, pos.y, pos.z, fwd.x, fwd.y, fwd.z);

		target = fwd + pos;
	}
#elif 0

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
	// const mat4 rot_mat_y = rotate(mat4::identity, angle.y, right);
	const mat4 rot_mat_y = rotate(mat4::identity, angle.y, vec3(1, 0, 0));

	pos = (rot_mat_x * (pos - target)) + target;
	//pos = (rot_mat_y * (pos - target)) + target;

	info("pos: " VFMT ", fwd: " VFMT ", right: " VFMT, V(pos), V(fwd), V(right));
	//updateView(pos, target, up);
#endif

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

	fwd = norm(target - pos);
	right = cross(fwd, vec3(0, 1, 0));

	// if the camera is exactly up
	const float cos_angle = dot(fwd, up);
	if (cos_angle * copysignf(1.f, angle.y) > 0.99f) {
		angle.y = 0.f;
	}

	const mat4 rot_mat_x = rotate(mat4::identity, angle.x, vec3(0, 1, 0));
	const mat4 rot_mat_y = rotate(mat4::identity, angle.y, right);
	//const mat4 rot_mat_y = rotate(mat4::identity, angle.y, vec3(1, 0, 0));

	const auto update_vec = [&]() {
		//pos = target - fwd * 200.f;
		fwd = norm(target - pos);
		right = norm(cross(fwd, vec3(0, 1, 0)));
		up = norm(cross(right, fwd));
	};

	//fwd = rot_mat_x * fwd;
	pos = rot_mat_x * pos;
	update_vec();
	//fwd = rot_mat_y * fwd;
	pos = rot_mat_y * pos;
	//if (fwd.z > -0.001f) fwd.z = -0.001f;
	update_vec();
	return;
	fwd = rot_mat_y * (rot_mat_x * fwd);
	if (fwd.z > -0.001f) fwd.z = -0.001f;
	//if (fwd.z < 0.001f) fwd.z = 0.001f;
	pos = target - fwd * 200.f;
	//fwd.y = -1.f;
	right = norm(cross(fwd, vec3(0, 1, 0)));
	up = norm(cross(right, fwd));

	info("fwd: " VFMT, V(fwd));
	//pos = (rot_mat_x * (pos - target)) + target;
	//const vec3 final_pos = (rot_mat_y * (pos - target)) + target;

	//updateView(pos, target, vec3(0, 1, 0));
#endif
}

void Camera::updateView(const vec3& eye, const vec3& lookat, const vec3& new_up) {
	pos = eye;
	target = lookat;
	up = new_up;
	//mat4 view_mat = lookAt(pos, target, up);
	//view = view_mat.data;

	fwd = norm(target - pos);
	right = norm(cross(fwd, vec3(0, 1, 0)));
	up = norm(cross(right, fwd));
}