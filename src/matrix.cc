#include "matrix.h"

#include "maths.h"

const matrix matrix::identity = {
	1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 1, 0,
	0, 0, 0, 1
};

const matrix matrix::zero = {
    0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0
};

matrix matrix::orthographic(float left, float right, float bottom, float top, float near, float far) {
	matrix out = matrix::zero;

	out[0][0] = 2.f / (right - left);
	out[1][1] = 2.0f / (top - bottom);
	out[2][2] = 2.0f / (near - far);
	out[3][3] = 1.0f;

	out[3][0] = (left + right) / (left - right);
	out[3][1] = (bottom + top) / (bottom - top);
	out[3][2] = (far + near) / (near - far);
	
	return out;
}

matrix matrix::perspective(float fov, float aspect_ratio, float near, float far) {
	matrix out = matrix::zero;

	float cotangent = 1.0f / tanf(fov * (math::pi / 360.0f));

	out[0][0] = cotangent / aspect_ratio;
	out[1][1] = cotangent;
	out[2][3] = -1.0f;
	out[2][2] = (near + far) / (near - far);
	out[3][2] = (2.0f * near * far) / (near - far);
	out[3][3] = 0.0f;

	return out;
}

matrix matrix::fromPos(vec3 pos) {
	matrix out = matrix::identity;

	out[3][0] = pos.x;
	out[3][1] = pos.y;
	out[3][2] = pos.z;

	return out;
}

matrix matrix::fromRot(float angle, vec3 axis) {
	matrix out = matrix::identity;

	axis.normalise();

	float sin_theta = sinf(angle);
	float cos_theta = cosf(angle);
	float cos_value = 1.0f - cos_theta;

	out[0][0] = (axis.x * axis.x * cos_value) + cos_theta;
	out[0][1] = (axis.x * axis.y * cos_value) + (axis.z * sin_theta);
	out[0][2] = (axis.x * axis.z * cos_value) - (axis.y * sin_theta);

	out[1][0] = (axis.y * axis.x * cos_value) - (axis.z * sin_theta);
	out[1][1] = (axis.y * axis.y * cos_value) + cos_theta;
	out[1][2] = (axis.y * axis.z * cos_value) + (axis.x * sin_theta);
	
	out[2][0] = (axis.z * axis.x * cos_value) + (axis.y * sin_theta);
	out[2][1] = (axis.z * axis.y * cos_value) - (axis.x * sin_theta);
	out[2][2] = (axis.z * axis.z * cos_value) + cos_theta;

	return out;
}

matrix matrix::fromRotX(float angle) {
	matrix out = matrix::identity;

	float c = cosf(angle);
	float s = sinf(angle);

	out[1][1] = c;
	out[1][2] = s;

	out[2][1] = -s;
	out[2][2] = c;

	return out;
}

matrix matrix::fromRotY(float angle) {
	matrix out = matrix::identity;

	float c = cosf(angle);
	float s = sinf(angle);

	out[0][0] = c;
	out[0][2] = -s;

	out[2][0] = s;
	out[2][2] = c;

	return out;
}

matrix matrix::fromRotZ(float angle) {
	matrix out = matrix::identity;

	float c = cosf(angle);
	float s = sinf(angle);

	out[0][0] = c;
	out[0][1] = s;

	out[1][0] = -s;
	out[1][1] = c;

	return out;
}

matrix matrix::fromScale(vec3 scale) {
	matrix out = matrix::identity;

	out[0][0] = scale.x;
	out[1][1] = scale.y;
	out[2][2] = scale.z;

	return out;
}

matrix matrix::transpose() const {
	matrix out = *this;
	for (int c = 0; c < 4; ++c) {
		for (int r = 0; r < 4; ++r) {
			out.rows[r][c] = rows[c][r];
		}
	}
	return out;
}
