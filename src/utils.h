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

	// returns a formatted string, can generate maximum 16 strings at a time before reusing buffers
	const char *format(const char *fmt, ...);
	char *formatBuf(char *src, size_t srclen, const char *fmt, ...);
	char *formatBufv(char *src, size_t srclen, const char *fmt, va_list args);
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

template<typename T>
struct dxptr {
	dxptr() = default;
	dxptr(T *ptr) : ptr(ptr) {}
	dxptr(dxptr &&other) { *this = std::move(other); }
	dxptr &operator=(dxptr &&other) { std::swap(ptr, other.ptr); }
	~dxptr() { if (ptr) { ptr->Release(); ptr = nullptr; } }

	T *get() { return ptr; }
	const T *get() const { return ptr; }

	operator bool() const { return ptr != nullptr; }
	T *operator->() { return ptr; }
	const T *operator->() const { return ptr; }

private:
	T *ptr = nullptr;
};