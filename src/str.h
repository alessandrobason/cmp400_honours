#pragma once

#include <limits.h>
#include <assert.h>

#include "mem.h"
#include "slice.h"
#include "common.h"

namespace str {
	// TCHAR string
	struct tstr {
		tstr() = default;
		tstr(char *cstr, size_t len = 0);
		tstr(const char *cstr, size_t len = 0);
		tstr(wchar_t *wstr, size_t len = 0);
		tstr(const wchar_t *wstr, size_t len = 0);
		~tstr();
		tstr(tstr &&t);
		tstr &operator=(tstr &&t);

		bool operator==(const tstr &t);

		size_t len() const;
		mem::ptr<char[]> toAnsi() const;
		mem::ptr<wchar_t[]> toWide() const;
		tstr clone() const;

		operator const TCHAR *() const;

		const TCHAR *buf = nullptr;
		bool is_owned = false;
	};

	struct view : public Slice<char> {
		using Super = typename Slice<char>;
		using Super::Super;

		view(const char *cstr, size_t len = 0);
		template<size_t size>
		view(const char (&data)[size]) : view(data) {}
		view(Super slice);

		view sub(size_t start = 0, size_t end = SIZE_MAX) const;

		void removePrefix(size_t count);
		void removeSuffix(size_t count);
		bool startsWith(view v) const;
		view trim() const;

		bool compare(view v) const;
		mem::ptr<char[]> dup() const;
		size_t findFirstOf(view values) const;
		size_t findLastOf(view values) const;

		size_t findFirstOf(char value) const;
		size_t findLastOf(char value) const;

		bool operator==(view v) const;
		bool operator!=(view v) const;
	};

	bool ncmp(const char *a, size_t alen, const char *b, size_t blen);
	bool cmp(const char *a, const char *b);

	mem::ptr<wchar_t[]> ansiToWide(const char *cstr, size_t len = 0);
	bool ansiToWide(const char *cstr, size_t src_len, wchar_t *buf, size_t dst_len);

	mem::ptr<char[]> wideToAnsi(const wchar_t *wstr, size_t len = 0);
	bool wideToAnsi(const wchar_t *wstr, size_t src_len, char *buf, size_t dst_len);

	mem::ptr<char[]> formatStr(const char *fmt, ...);

	mem::ptr<char[]> dup(const char *cstr, size_t len = 0);

	int toInt(const char *cstr);
	unsigned int toUInt(const char *cstr);
	double toNum(const char *cstr);

	// returns a formatted string, can generate maximum 16 strings at a time before reusing buffers
	const char *format(const char *fmt, ...);
	// const char *formatV(const char *fmt, va_list args);
	char *formatBuf(char *src, size_t srclen, const char *fmt, ...);
	char *formatBufv(char *src, size_t srclen, const char *fmt, va_list args);
} // namespace str 