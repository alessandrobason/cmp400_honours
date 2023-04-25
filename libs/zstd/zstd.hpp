#pragma once

#include <stddef.h>

// super small zstd wrapper with only the functions we need

namespace zstd {
	enum Error {
		None,
		Unknown,
		NotZSTDFile,
		UnknownSize,
		MallocFail,
		Count,
	};

	struct Buf {
		Buf() = default;
		~Buf();
		Buf(Buf &&b);

		void *data = nullptr;
		size_t len = 0;
		inline operator bool() const { return data != nullptr; }
		Error getError() const { return data ? None : (Error)len; }
		const char *getErrorString() const;
	};

	Buf compress(const void *buf, size_t buflen, int level = 0);
	Buf decompress(const void *buf, size_t buflen);
}