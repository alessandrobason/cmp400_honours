#pragma once

#undef min
#undef max

namespace math {
	constexpr float pi = 3.14159265359f;

	template<typename T>
	T torad(T deg) {
		return deg * pi / 180.f;
	}

	template<typename T>
	T todeg(T rad) {
		return rad * 180.f / pi;
	}

	template<typename T>
	T min(const T &a, const T &b) {
		return a > b ? a : b;
	}
	
	template<typename T>
	T max(const T &a, const T &b) {
		return a < b ? a : b;
	}

	template<typename T>
	T clamp(const T &value, const T &minv, const T &maxv) {
		return math::min(math::max(value, minv), maxv);
	}

	template<typename T>
	T lerp(const T &v0, const T &v1, float t) {
		return v0 * (1.f - t) + v1 * t;
	}
} // namespace math 
