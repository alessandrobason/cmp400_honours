#include "str_utils.h"

#include <stdarg.h>
#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "tracelog.h"

namespace str {
	std::unique_ptr<wchar_t[]> ansiToWide(const char *cstr, size_t len) {
		if (len == 0) len = strlen(cstr);
		int result_len = MultiByteToWideChar(CP_UTF8, 0, cstr, (int)len, NULL, 0);
		std::unique_ptr<wchar_t[]> wstr = std::make_unique<wchar_t[]>(result_len);
		result_len = MultiByteToWideChar(CP_UTF8, 0, cstr, (int)len, wstr.get(), result_len);
		wstr[result_len] = '\0';
		return wstr;
	}

	bool ansiToWide(const char *cstr, size_t src_len, wchar_t *buf, size_t dst_len) {
		if (src_len == 0) src_len = strlen(cstr);
		int result_len = MultiByteToWideChar(CP_UTF8, 0, cstr, (int)src_len, buf, (int)dst_len);
		return result_len <= dst_len;
	}

	std::unique_ptr<char[]> wideToAnsi(const wchar_t *wstr, size_t len) {
		if (len == 0) len = wcslen(wstr);
		int result_len = WideCharToMultiByte(CP_UTF8, 0, wstr, (int)len, NULL, 0, NULL, NULL);
		std::unique_ptr<char[]> cstr = std::make_unique<char[]>(result_len);
		result_len = WideCharToMultiByte(CP_UTF8, 0, wstr, (int)len, cstr.get(), result_len, NULL, NULL);
		cstr[result_len] = '\0';
		return cstr;
	}

	bool wideToAnsi(const wchar_t *wstr, size_t src_len, char *buf, size_t dst_len) {
		if (src_len == 0) src_len = wcslen(wstr);
		int result_len = WideCharToMultiByte(CP_UTF8, 0, wstr, (int)src_len, buf, (int)dst_len, NULL, NULL);
		return result_len <= dst_len;
	}

	std::unique_ptr<char[]> formatStr(const char *fmt, ...) {
		std::unique_ptr<char[]> out;
		va_list va, vtemp;
		va_start(va, fmt);

		va_copy(vtemp, va);
		int len = vsnprintf(nullptr, 0, fmt, vtemp);
		va_end(vtemp);
		
		if (len < 0) {
			err("couldn't format string \"%s\"", fmt);
			return out;
		}

		out = std::make_unique<char[]>(len + 1);
		len = vsnprintf(out.get(), len + 1, fmt, va);
		va_end(va);

		return out;
	}

	char *format(char *src, size_t srclen, const char *fmt, ...) {
		va_list va;
		va_start(va, fmt);
		char* str = formatv(src, srclen, fmt, va);
		va_end(va);
		return str;
	}

	char *formatv(char *src, size_t srclen, const char *fmt, va_list args) {
		int len = vsnprintf(src, srclen, fmt, args);
		if (len >= srclen) {
			len = srclen - 1;
		}
		src[len] = '\0';
		return src;
	}
} // namespace str 
