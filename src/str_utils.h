#pragma once

#include <tchar.h>
#include <memory>

namespace str {
	std::unique_ptr<wchar_t[]> ansiToWide(const char *cstr, size_t len = 0);
	bool ansiToWide(const char *cstr, size_t src_len, wchar_t *buf, size_t dst_len);
	
	std::unique_ptr<char[]> wideToAnsi(const wchar_t *wstr, size_t len = 0);
	bool wideToAnsi(const wchar_t *wstr, size_t src_len, char *buf, size_t dst_len);

	template<typename T>
	void memzero(T &value) { memset(&value, 0, sizeof(T)); }

	template<typename T>
	void memcopy(T &dst, const T &src) { memcpy(&dst, &src, sizeof(T)); }

	std::unique_ptr<char[]> format(const char *fmt, ...);
} // namespace str 
