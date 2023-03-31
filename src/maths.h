#pragma once

#undef min
#undef max

namespace math {
	constexpr float pi = 3.14159265359f;

	template<typename T>
	constexpr T torad(T deg) {
		return deg * pi / 180.f;
	}

	template<typename T>
	constexpr T todeg(T rad) {
		return rad * 180.f / pi;
	}

	template<typename T>
	constexpr T min(const T &a, const T &b) {
		return a < b ? a : b;
	}
	
	template<typename T>
	constexpr T max(const T &a, const T &b) {
		return a > b ? a : b;
	}

	template<typename T>
	constexpr T clamp(const T &value, const T &minv, const T &maxv) {
		return math::min(math::max(value, minv), maxv);
	}

	template<typename T>
	constexpr T lerp(const T &v0, const T &v1, float t) {
		return v0 * (1.f - t) + v1 * t;
	}
} // namespace math 
