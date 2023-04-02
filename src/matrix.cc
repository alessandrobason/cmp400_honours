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


constexpr matrix::matrix()
	: m00(1), m01(0), m02(0), m03(0),
	  m10(0), m11(1), m12(0), m13(0),
	  m20(0), m21(0), m22(1), m23(0),
	  m30(0), m31(0), m32(0), m33(1) 
{
}

constexpr matrix::matrix(float mat[16])
	: m00(mat[0]),  m01(mat[1]),  m02(mat[2]),  m03(mat[3]),
	  m10(mat[4]),  m11(mat[5]),  m12(mat[6]),  m13(mat[7]),
	  m20(mat[8]),  m21(mat[9]),  m22(mat[10]), m23(mat[11]),
	  m30(mat[12]), m31(mat[13]), m32(mat[14]), m33(mat[15]) 
{
}

constexpr matrix::matrix(
	float a00, float a01, float a02, float a03, 
	float a10, float a11, float a12, float a13, 
	float a20, float a21, float a22, float a23, 
	float a30, float a31, float a32, float a33
)   
	: m00(a00), m01(a01), m02(a02), m03(a03), 
	  m10(a10), m11(a11), m12(a12), m13(a13), 
	  m20(a20), m21(a21), m22(a22), m23(a23), 
	  m30(a30), m31(a31), m32(a32), m33(a33) 
{
}

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

	axis.normalised();

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
