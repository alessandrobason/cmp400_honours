#pragma once

#include <tchar.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include <memory>
#include <string>

// == string utils ================================================
namespace str {
	std::unique_ptr<wchar_t[]> ansiToWide(const char *cstr, size_t len = 0);
	bool ansiToWide(const char *cstr, size_t src_len, wchar_t *buf, size_t dst_len);

	std::unique_ptr<char[]> wideToAnsi(const wchar_t *wstr, size_t len = 0);
	bool wideToAnsi(const wchar_t *wstr, size_t src_len, char *buf, size_t dst_len);

	std::unique_ptr<char[]> formatStr(const char *fmt, ...);

	char *format(char *src, size_t srclen, const char *fmt, ...);
	char *formatv(char *src, size_t srclen, const char *fmt, va_list args);
} // namespace str 

// == file utils ==================================================
namespace file {
	struct MemoryBuf {
		std::unique_ptr<uint8_t[]> data = nullptr;
		size_t size = 0;
	};

	bool exists(const char *filename);
	size_t getSize(FILE *fp);

	MemoryBuf read(const char *filename);
	MemoryBuf read(FILE *fp);
	std::string readString(const char *filename);
	std::string readString(FILE *fp);
} // namespace file 

// == memory utils ================================================
namespace mem {
	template<typename T>
	void zero(T &value) { memset(&value, 0, sizeof(T)); }
	
	template<typename T>
	void copy(T &dst, const T &src) { memcpy(&dst, &src, sizeof(T)); }
} // namespace mem
