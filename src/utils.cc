#include "utils.h"

#include <assert.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "tracelog.h"

// == string utils ================================================
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
		if (result_len <= dst_len) {
			buf[result_len] = '\0';
			return true;
		}
		return false;
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

		out = std::make_unique<char[]>((size_t)len + 1);
		len = vsnprintf(out.get(), (size_t)len + 1, fmt, va);
		va_end(va);

		return out;
	}

	const char *format(const char *fmt, ...) {
		static char static_buf[16][1024];
		static int cur_buf = 0;
		mem::zero(static_buf[cur_buf]);
		va_list va;
		va_start(va, fmt);
		char *str = formatBufv(static_buf[cur_buf], sizeof(static_buf[cur_buf]), fmt, va);
		va_end(va);
		if ((++cur_buf) >= sizeof(static_buf)) cur_buf = 0;
		return str;
	}

	char *formatBuf(char *src, size_t srclen, const char *fmt, ...) {
		va_list va;
		va_start(va, fmt);
		char* str = formatBufv(src, srclen, fmt, va);
		va_end(va);
		return str;
	}

	char *formatBufv(char *src, size_t srclen, const char *fmt, va_list args) {
		int len = vsnprintf(src, srclen, fmt, args);
		if (len >= srclen) {
			len = (int)srclen - 1;
		}
		src[len] = '\0';
		return src;
	}
} // namespace str 

// == file utils ==================================================

namespace file {
	bool exists(const char *filename) {
		return GetFileAttributesA(filename) != INVALID_FILE_ATTRIBUTES;
	}

	size_t getSize(FILE *fp) {
		if (!fp) {
			err("invalid FILE handler passed to getSize");
			return 0;
		}
		fseek(fp, 0, SEEK_END);
		long len = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		return (size_t)len;
	}

	MemoryBuf read(const char *filename) {
		FILE *fp = fopen(filename, "rb");
		if (!fp) {
			err("couldn't open file: %s", filename);
			return {};
		}
		return read(fp);
	}

	MemoryBuf read(FILE *fp) {
		if (!fp) {
			err("invalid FILE handler passed to read");
			return {};
		}

		MemoryBuf out;

		out.size = getSize(fp);
		out.data = std::make_unique<uint8_t[]>(out.size);
		size_t read = fread(out.data.get(), 1, out.size, fp);

		assert(read == out.size);
		if (read != out.size) {
			out = {};
		}

		return out;
	}

	std::string readString(const char *filename) {
		FILE *fp = fopen(filename, "rb");
		if (!fp) {
			err("couldn't open file: %s", filename);
			return {};
		}
		return readString(fp);
	}

	std::string readString(FILE *fp) {
		if (!fp) {
			err("invalid FILE handler passed to readString");
			return {};
		}

		std::string out;
		out.resize(getSize(fp));
		size_t read = fread(&out[0], 1, out.size(), fp);

		assert(read == out.size());
		if (read != out.size()) {
			out.clear();
		}

		return out;
	}

} // namespace file 