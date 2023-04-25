#include "zstd.hpp"

#include <stdlib.h>

// we compiled zstd in a single file lib, we only need to include the source file
// once and that will build the whole lib
#include "lib/zstd.c"

template<typename T>
void swap(T &a, T &b) {
	T temp = a;
	a = b;
	b = temp;
}

namespace zstd {
	Buf::~Buf() {
		free(data);
		data = nullptr;
		len = 0;
	}

	Buf::Buf(Buf &&b) {
		if (this != &b) {
			swap(data, b.data);
			swap(len, b.len);
		}
	}

	const char *Buf::getErrorString() const {
		static const char *error_strings[Count] = {
			"No Error",                     // None
			"Unknown Error",                // Unknown
			"File is not an ZSTD file",     // NotZSTDFile
			"Could not get size from file", // UnknownSize
			"Out of Memory",                // MallocFail
		};
		return error_strings[getError()];
	}


	Buf compress(const void *buf, size_t buflen, int level) {
		Buf out;
		out.len = ZSTD_compressBound(buflen);
		out.data = malloc(out.len);
		if (!out.data) {
			out.len = MallocFail;
			return out;
		}

		out.len = ZSTD_compress(out.data, out.len, buf, buflen, level);
		
		return out;
	}

	Buf decompress(const void *buf, size_t buflen) {
		Buf out;

		out.len = ZSTD_getFrameContentSize(buf, buflen);
		if (out.len == ZSTD_CONTENTSIZE_ERROR) {
			out.len = NotZSTDFile;
			return out;
		}
		else if (out.len == ZSTD_CONTENTSIZE_UNKNOWN) {
			out.len = UnknownSize;
			return out;
		}

		out.data = malloc(out.len);
		if (!out.data) {
			out.len = MallocFail;
			return out;
		}

		size_t outsize = ZSTD_decompress(out.data, out.len, buf, buflen);
		if (outsize != out.len) {
			free(out.data);
			out.data = nullptr;
			out.len = Unknown;
			return out;
		}

		return out;
	}
}
