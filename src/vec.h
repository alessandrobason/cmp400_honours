#pragma once

#include <math.h>

#pragma warning(push)
#pragma warning(disable: 4201) // nonstandard extension used: nameless struct/union

template<typename T>
struct vec2T {
	union {
		T data[2];
		struct { T x, y; };
		struct { T u, v; };
	};

	constexpr vec2T() : x(0), y(0) {}
	constexpr vec2T(T v) : x(v), y(v) {}
	constexpr vec2T(T x, T y) : x(x), y(y) {}
	template<typename Q>
	constexpr vec2T(const vec2T<Q> &v) : x((T)v.x), y((T)v.y) {}

	vec2T operator+(const vec2T &o) const { return { x + o.x, y + o.y }; }
	vec2T operator-(const vec2T &o) const { return { x - o.x, y - o.y }; }
	vec2T operator*(const vec2T &o) const { return { x * o.x, y * o.y }; }
	vec2T operator/(const vec2T &o) const { return { x / o.x, y / o.y }; }

	vec2T &operator+=(const vec2T &o) { x += o.x; y += o.y; return *this; }
	vec2T &operator-=(const vec2T &o) { x -= o.x; y -= o.y; return *this; }
	vec2T &operator*=(const vec2T &o) { x *= o.x; y *= o.y; return *this; }
	vec2T &operator/=(const vec2T &o) { x /= o.x; y /= o.y; return *this; }

	vec2T operator+(T o) const { return { x + o, y + o }; }
	vec2T operator-(T o) const { return { x - o, y - o }; }
	vec2T operator*(T o) const { return { x * o, y * o }; }
	vec2T operator/(T o) const { return { x / o, y / o }; }

	vec2T &operator+=(T o) { x += o; y += o; return *this; }
	vec2T &operator-=(T o) { x -= o; y -= o; return *this; }
	vec2T &operator*=(T o) { x *= o; y *= o; return *this; }
	vec2T &operator/=(T o) { x /= o; y /= o; return *this; }

	T mag2() const { return x * x + y * y; }
	T mag() const { return sqrt(mag2()); }
	void norm() { T m = mag(); if (m) *this /= m; }
	vec2T normalise() const { vec2T v = *this; v.norm(); return v; }
};

template<typename T>
struct vec3T {
	union {
		T data[3];
		struct { T x, y, z; };
		struct { T r, g, b; };
		struct { vec2T<T> v; T z; };
	};

	constexpr vec3T() : x(0), y(0), z(0) {}
	constexpr vec3T(T v) : x(v), y(v), z(v) {}
	constexpr vec3T(T x, T y, T z) : x(x), y(y), z(z) {}
	constexpr vec3T(const vec2T<T> &v, T z) : v(v), z(z) {}
	template<typename Q>
	constexpr vec3T(const vec3T<Q> &v) : x((T)v.x), y((T)v.y), z((T)v.z) {}

	vec3T operator+(const vec3T &o) const { return { x + o.x, y + o.y, z + o.z }; }
	vec3T operator-(const vec3T &o) const { return { x - o.x, y - o.y, z - o.z }; }
	vec3T operator*(const vec3T &o) const { return { x * o.x, y * o.y, z * o.z }; }
	vec3T operator/(const vec3T &o) const { return { x / o.x, y / o.y, z / o.z }; }

	vec3T &operator+=(const vec3T &o) { x += o.x; y += o.y; z += o.z; return *this; }
	vec3T &operator-=(const vec3T &o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
	vec3T &operator*=(const vec3T &o) { x *= o.x; y *= o.y; z *= o.z; return *this; }
	vec3T &operator/=(const vec3T &o) { x /= o.x; y /= o.y; z /= o.z; return *this; }

	vec3T operator+(T o) const { return { x + o, y + o, z + o }; }
	vec3T operator-(T o) const { return { x - o, y - o, z - o }; }
	vec3T operator*(T o) const { return { x * o, y * o, z * o }; }
	vec3T operator/(T o) const { return { x / o, y / o, z / o }; }

	vec3T &operator+=(T o) { x += o; y += o; z += o; return *this; }
	vec3T &operator-=(T o) { x -= o; y -= o; z -= o; return *this; }
	vec3T &operator*=(T o) { x *= o; y *= o; z *= o; return *this; }
	vec3T &operator/=(T o) { x /= o; y /= o; z /= o; return *this; }

	T mag2() const { return x * x + y * y + z * z; }
	T mag() const { return (T)sqrt(mag2()); }
	void norm() { T m = mag(); if (m) *this /= m; }
	vec3T normalise() const { vec3T o = *this; o.norm(); return o; }
};

template<typename T>
struct vec4T {
	union {
		T data[4];
		struct { T x, y, z, w; };
		struct { T x, y, w, h; };
		struct { T r, g, b, a; };
		struct { vec2T<T> pos, size; };
		struct { vec3T<T> v; T w; };
	};

	constexpr vec4T() : x(0), y(0), z(0), w(0) {}
	constexpr vec4T(T v) : x(v), y(v), z(v), w(v) {}
	constexpr vec4T(T x, T y, T z, T w) : x(x), y(y), z(z), w(w) {}
	constexpr vec4T(const vec2T<T> &pos, const vec2T<T> &size) : pos(pos), size(size) {}
	constexpr vec4T(const vec3T<T> &v, T w) : v(v), w(w) {}
	template<typename Q>
	constexpr vec4T(const vec4T<Q> &v) : x((T)v.x), y((T)v.y), z((T)v.z), w((T)v.w) {}

	vec4T operator+(const vec4T &o) const { return { x + o.x, y + o.y, z + o.z, w + o.w }; }
	vec4T operator-(const vec4T &o) const { return { x - o.x, y - o.y, z - o.z, w - o.w }; }
	vec4T operator*(const vec4T &o) const { return { x * o.x, y * o.y, z * o.z, w * o.w }; }
	vec4T operator/(const vec4T &o) const { return { x / o.x, y / o.y, z / o.z, w / o.w }; }

	vec4T &operator+=(const vec4T &o) { x += o.x; y += o.y; z += o.z; w += o.w; return *this; }
	vec4T &operator-=(const vec4T &o) { x -= o.x; y -= o.y; z -= o.z; w -= o.w; return *this; }
	vec4T &operator*=(const vec4T &o) { x *= o.x; y *= o.y; z *= o.z; w *= o.w; return *this; }
	vec4T &operator/=(const vec4T &o) { x /= o.x; y /= o.y; z /= o.z; w /= o.w; return *this; }

	vec4T operator+(T o) const { return { x + o, y + o, z + o, w + o }; }
	vec4T operator-(T o) const { return { x - o, y - o, z - o, w - o }; }
	vec4T operator*(T o) const { return { x * o, y * o, z * o, w * o }; }
	vec4T operator/(T o) const { return { x / o, y / o, z / o, w / o }; }

	vec4T &operator+=(T o) { x += o; y += o; z += o; w += o; return *this; }
	vec4T &operator-=(T o) { x -= o; y -= o; z -= o; w -= o; return *this; }
	vec4T &operator*=(T o) { x *= o; y *= o; z *= o; w *= o; return *this; }
	vec4T &operator/=(T o) { x /= o; y /= o; z /= o; w /= o; return *this; }

	T mag2() const { return x * x + y * y + z * z + w * w; }
	T mag() const { return sqrt(mag2()); }
	void norm() { T m = mag(); if (m) *this /= m; }
	vec4T normalise() const { vec4T o = *this; o.norm(); return o; }
};

using vec2 = vec2T<float>;
using vec3 = vec3T<float>;
using vec4 = vec4T<float>;

using vec2d = vec2T<double>;
using vec3d = vec3T<double>;
using vec4d = vec4T<double>;

using vec2i = vec2T<int>;
using vec3i = vec3T<int>;
using vec4i = vec4T<int>;

using vec2u = vec2T<unsigned int>;
using vec3u = vec3T<unsigned int>;
using vec4u = vec4T<unsigned int>;

#pragma warning(pop)
