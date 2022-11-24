#pragma once

#include <math.h>
#include <assert.h>

#include "vec.h"

#pragma warning(push)
#pragma warning(disable: 4201) // nonstandard extension used: nameless struct/union

union matrix {
	float data[16];
	float rows[4][4];
	struct {
		float m00, m01, m02, m03;
		float m10, m11, m12, m13;
		float m20, m21, m22, m23;
		float m30, m31, m32, m33;
	};

	static const matrix identity;
	static const matrix zero;

	constexpr matrix() 
		: m00(1), m01(0), m02(0), m03(0), 
		  m10(0), m11(1), m12(0), m13(0), 
		  m20(0), m21(0), m22(1), m23(0), 
		  m30(0), m31(0), m32(0), m33(1) {}
	constexpr matrix(float mat[16]) 
		: m00(mat[0]),  m01(mat[1]),  m02(mat[2]),  m03(mat[3]), 
		  m10(mat[4]),  m11(mat[5]),  m12(mat[6]),  m13(mat[7]), 
		  m20(mat[8]),  m21(mat[9]),  m22(mat[10]), m23(mat[11]), 
		  m30(mat[12]), m31(mat[13]), m32(mat[14]), m33(mat[15]) {}
	constexpr matrix(
		float a00, float a01, float a02, float a03, 
		float a10, float a11, float a12, float a13, 
		float a20, float a21, float a22, float a23, 
		float a30, float a31, float a32, float a33
	)   : m00(a00), m01(a01), m02(a02), m03(a03), 
		  m10(a10), m11(a11), m12(a12), m13(a13), 
		  m20(a20), m21(a21), m22(a22), m23(a23), 
		  m30(a30), m31(a31), m32(a32), m33(a33) {}

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
