#pragma once

#include <math.h>
#include <assert.h>

#include "vec.h"

#pragma warning(push)
#pragma warning(disable: 4201) // nonstandard extension used: nameless struct/union

struct matrix {
	union {
		float data[16];
		float rows[4][4];
		struct {
			float m00, m01, m02, m03;
			float m10, m11, m12, m13;
			float m20, m21, m22, m23;
			float m30, m31, m32, m33;
		};
	};

	static const matrix identity;
	static const matrix zero;

	constexpr matrix();
	constexpr matrix(float mat[16]);
	constexpr matrix(
		float a00, float a01, float a02, float a03,
		float a10, float a11, float a12, float a13,
		float a20, float a21, float a22, float a23,
		float a30, float a31, float a32, float a33
	);

	static matrix orthographic(float left, float right, float bottom, float top, float near, float far);
	static matrix perspective(float fov, float aspect_ratio, float near, float far);

	static matrix fromPos(vec3 pos);
	static matrix fromRot(float angle, vec3 axis);
	static matrix fromRotX(float angle);
	static matrix fromRotY(float angle);
	static matrix fromRotZ(float angle);
	static matrix fromScale(vec3 scale);

	matrix transpose() const;

	float *operator[](size_t i) {
		assert(i < 4);
		return rows[i];
	}

	const float *operator[](size_t i) const {
		assert(i < 4);
		return rows[i];
	}

#define MAT_OP(op) \
	m00 op m.m00, m01 op m.m01, m02 op m.m02, m03 op m.m03, \
	m10 op m.m10, m11 op m.m11, m12 op m.m12, m13 op m.m13, \
	m20 op m.m20, m21 op m.m21, m22 op m.m22, m23 op m.m23, \
	m30 op m.m30, m31 op m.m31, m32 op m.m32, m33 op m.m33

	matrix operator+(const matrix &m) const { return { MAT_OP(+) }; }
	matrix operator-(const matrix &m) const { return { MAT_OP(-) }; }
	matrix operator*(const matrix &m) const { return { MAT_OP(*) }; }
	matrix operator/(const matrix &m) const { return { MAT_OP(/) }; }

	matrix &operator+=(const matrix &m) { MAT_OP(+=); return *this; }
	matrix &operator-=(const matrix &m) { MAT_OP(-=); return *this; }
	matrix &operator*=(const matrix &m) { MAT_OP(*=); return *this; }
	matrix &operator/=(const matrix &m) { MAT_OP(/=); return *this; }

#undef MAT_OP

#define MAT_OP(op) \
	m00 op v, m01 op v, m02 op v, m03 op v, \
	m10 op v, m11 op v, m12 op v, m13 op v, \
	m20 op v, m21 op v, m22 op v, m23 op v, \
	m30 op v, m31 op v, m32 op v, m33 op v

	matrix operator+(float v) const { return { MAT_OP(+) }; }
	matrix operator-(float v) const { return { MAT_OP(-) }; }
	matrix operator*(float v) const { return { MAT_OP(*) }; }
	matrix operator/(float v) const { return { MAT_OP(/) }; }

	matrix &operator+=(float v) { MAT_OP(+=); return *this; }
	matrix &operator-=(float v) { MAT_OP(-=); return *this; }
	matrix &operator*=(float v) { MAT_OP(*=); return *this; }
	matrix &operator/=(float v) { MAT_OP(/=); return *this; }

#undef MAT_OP
};

#pragma warning(pop)
