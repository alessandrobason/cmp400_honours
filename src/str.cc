#include "str.h"

#include <string.h>
#include <wchar.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h> // HUGE_VAL

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "common.h"
#include "tracelog.h"

namespace str {
	view::view(const char *cstr, size_t len)
		: Super(cstr, cstr ? (len ? len : strlen(cstr)) : 0)
	{
	}

	view::view(Super slice)
		: Super(slice)
	{
	}

	view view::sub(size_t start, size_t end) const {
		return Slice::sub(start, end);
	}

	void view::removePrefix(size_t count) {
		if (count > len) count = len;
		data += count;
		len -= count;
	}

	void view::removeSuffix(size_t count) {
		if (count > len) count = len;
		len -= count;
	}

	bool view::startsWith(view v) const {
		if (v.len > len) return false;
		return memcmp(data, v.data, v.len) == 0;
	}

	view view::trim() const {
		if (empty()) return view();
		str::view out = *this;
		// trim left
		for (size_t i = 0; i < len && isspace(data[i]); ++i) {
			out.removePrefix(1);
		}
		if (out.empty()) return out;
		// trim right
		for (long long i = out.len - 1; i >= 0 && isspace(out[i]); --i) {
			out.removeSuffix(1);
		}
		return out;
	}

	bool view::compare(view v) const {
		return str::ncmp(data, len, v.data, v.len);
	}

	mem::ptr<char[]> view::dup() const {
		return str::dup(data, len);
	}

	size_t view::findFirstOf(view values) const {
		for (size_t i = 0; i < len; ++i) {
			for (size_t k = 0; k < values.len; ++k) {
				if (data[i] == values[k]) return i;
			}
		}
		return SIZE_MAX;
	}

	size_t view::findLastOf(view values) const {
		for (ssize_t i = len - 1; i >= 0; --i) {
			for (size_t k = 0; k < values.len; ++k) {
				if (data[i] == values[k]) return i;
			}
		}
		return SIZE_MAX;
	}

	size_t view::findFirstOf(char value) const {
		for (size_t i = 0; i < len; ++i) {
			if (data[i] == value) return i;
		}
		return SIZE_MAX;
	}

	size_t view::findLastOf(char value) const {
		for (ssize_t i = len - 1; i >= 0; --i) {
			if (data[i] == value) return i;
		}
		return SIZE_MAX;
	}

	bool view::operator==(view v) const {
		return compare(v);
	}

	bool view::operator!=(view v) const {
		return !compare(v);
	}

	bool ncmp(const char *a, size_t alen, const char *b, size_t blen) {
		if (!a && !b) return true;
		if (!a || !b) return false;
		if (alen != blen) return false;
		return memcmp(a, b, alen) == 0;
	}

	bool cmp(const char *a, const char *b) {
		if (!a && !b) return true;
		if (!a || !b) return false;
		return strcmp(a, b) == 0;
	}

	mem::ptr<wchar_t[]> ansiToWide(const char *cstr, size_t len) {
		if (len == 0) len = strlen(cstr);
		// MultiByteToWideChar returns length INCLUDING null-terminating character
		int result_len = MultiByteToWideChar(CP_UTF8, 0, cstr, (int)len, NULL, 0);
		mem::ptr<wchar_t[]> wstr = mem::ptr<wchar_t[]>::make(result_len + 1);
		result_len = MultiByteToWideChar(CP_UTF8, 0, cstr, (int)len, wstr.get(), result_len);
		wstr[result_len] = '\0';
		return wstr;
	}

	bool ansiToWide(const char *cstr, size_t src_len, wchar_t *buf, size_t dst_len) {
		if (src_len == 0) src_len = strlen(cstr);
		int result_len = MultiByteToWideChar(CP_UTF8, 0, cstr, (int)src_len, buf, (int)dst_len);
		if (result_len < dst_len) {
			buf[result_len] = '\0';
			return true;
		}
		return false;
	}

	mem::ptr<char[]> wideToAnsi(const wchar_t *wstr, size_t len) {
		if (len == 0) len = wcslen(wstr);
		int result_len = WideCharToMultiByte(CP_UTF8, 0, wstr, (int)len, NULL, 0, NULL, NULL);
		mem::ptr<char[]> cstr = mem::ptr<char[]>::make(result_len + 1);
		result_len = WideCharToMultiByte(CP_UTF8, 0, wstr, (int)len, cstr.get(), result_len, NULL, NULL);
		cstr[result_len] = '\0';
		return cstr;
	}

	bool wideToAnsi(const wchar_t *wstr, size_t src_len, char *buf, size_t dst_len) {
		if (src_len == 0) src_len = wcslen(wstr);
		int result_len = WideCharToMultiByte(CP_UTF8, 0, wstr, (int)src_len, buf, (int)dst_len, NULL, NULL);
		if (result_len < dst_len) {
			buf[result_len] = '\0';
			return true;
		}
		return false;
	}

	mem::ptr<char[]> formatStr(const char *fmt, ...) {
		mem::ptr<char[]> out;
		va_list va, vtemp;
		va_start(va, fmt);

		va_copy(vtemp, va);
		int len = vsnprintf(nullptr, 0, fmt, vtemp);
		va_end(vtemp);

		if (len < 0) {
			err("couldn't format string \"%s\"", fmt);
			return out;
		}

		out = mem::ptr<char[]>::make((size_t)len + 1);
		len = vsnprintf(out.get(), (size_t)len + 1, fmt, va);
		va_end(va);

		return out;
	}

	mem::ptr<char[]> dup(const char *cstr, size_t len) {
		if (!cstr) return nullptr;
		if (!len) len = strlen(cstr);
		auto ptr = mem::ptr<char[]>::make(len + 1);
		memcpy(ptr.get(), cstr, len);
		ptr[len] = '\0';
		return ptr;
	}

	int toInt(const char *cstr) {
		if (!cstr) return 0;
		int out = strtol(cstr, NULL, 0);
		if (out == LONG_MAX || out == LONG_MIN) {
			out = 0;
		}
		return out;
	}

	unsigned int toUInt(const char *cstr) {
		if (!cstr) return 0;
		unsigned int out = strtoul(cstr, NULL, 0);
		if (out == ULONG_MAX) {
			out = 0;
		}
		return out;
	}

	double toNum(const char *cstr) {
		if (!cstr) return 0;
		double out = strtod(cstr, NULL);
		if (out == HUGE_VAL || out == -HUGE_VAL) {
			out = 0;
		}
		return out;
	}

	const char *formatV(const char *fmt, va_list args) {
		static char static_buf[64][1024];
		static int cur_buf = 0;
		mem::zero(static_buf[cur_buf]);
		va_list vtemp;
		va_copy(vtemp, args);
		char *str = formatBufv(static_buf[cur_buf], sizeof(static_buf[cur_buf]), fmt, vtemp);
		va_end(vtemp);
		if ((++cur_buf) >= ARRLEN(static_buf)) cur_buf = 0;
		return str;
	}

	const char *format(const char *fmt, ...) {
		va_list va;
		va_start(va, fmt);
		const char *str = formatV(fmt, va);
		va_end(va);
		return str;
	}

	char *formatBuf(char *src, size_t srclen, const char *fmt, ...) {
		va_list va;
		va_start(va, fmt);
		char *str = formatBufv(src, srclen, fmt, va);
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

	tstr::tstr(char *cstr, size_t len) {
		if (!cstr) return;
#ifdef UNICODE
		buf = ansiToWide(cstr, len).release();
		is_owned = true;
#else
		UNUSED(len);
		buf = cstr;
#endif
	}

	tstr::tstr(const char *cstr, size_t len) {
		if (!cstr) return;
#ifdef UNICODE
		is_owned = true;
		buf = ansiToWide(cstr, len).release();
#else
		UNUSED(len);
		buf = cstr;
#endif
	}

	tstr::tstr(wchar_t *wstr, size_t len) {
		if (!wstr) return;
#ifdef UNICODE
		UNUSED(len);
		buf = wstr;
#else
		buf = wideToAnsi(wstr, len).release();
		is_owned = true;
#endif
	}

	tstr::tstr(const wchar_t *wstr, size_t len) {
		if (!wstr) return;
#ifdef UNICODE
		UNUSED(len);
		buf = wstr;
#else
		is_owned = true;
		buf = wideToAnsi(wstr, len).release();
#endif
	}

	tstr::~tstr() {
		if (is_owned) {
			delete[] buf;
		}
	}

	tstr::tstr(tstr &&t) {
		*this = mem::move(t);
	}

	tstr &tstr::operator=(tstr &&t) {
		if (this != &t) {
			mem::swap(buf, t.buf);
			mem::swap(is_owned, t.is_owned);
		}
		return *this;
	}

	bool tstr::operator==(const tstr &t) {
		if (!buf || !t.buf) return false;
#ifdef UNICODE
		return wcscmp(buf, t.buf) == 0;
#else
		return strcmp(buf, t.buf) == 0;
#endif
	}

	size_t tstr::len() const {
		if (!buf) return 0;
#ifdef UNICODE
		return wcslen(buf);
#else
		return strlen(buf);
#endif
	}

	mem::ptr<char[]> tstr::toAnsi() const {
		if (!buf) return nullptr;
#ifdef UNICODE
		return wideToAnsi(buf);
#else
		return dup(buf);
#endif
	}

	mem::ptr<wchar_t[]> tstr::toWide() const {
		if (!buf) return nullptr;
#ifdef UNICODE
		// str::dup for wstr
		size_t l = wcslen(buf);
		auto ptr = mem::ptr<wchar_t[]>::make(l + 1);
		memcpy(ptr.get(), buf, l);
		ptr[l] = '\0';
		return ptr;
#else
		return ansiToWide(buf);
#endif
	}

	tstr tstr::clone() const {
		return tstr(buf);
	}

	tstr::operator const TCHAR *() const {
		return buf;
	}
} // namespace str 