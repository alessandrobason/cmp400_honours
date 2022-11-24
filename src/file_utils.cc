#include "file_utils.h"

#include <Windows.h>

#include <assert.h>
#include "tracelog.h"

template<typename T>
static T readBuf(FILE *fp) {
	if (!fp) return {};
	T out{};

	fseek(fp, 0, SEEK_END);
	long len = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	out.resize(len);
	size_t read = fread(&out[0], 1, out.size(), fp);

	assert(read == out.size());
	if (read != out.size()) {
		out.clear();
	}

	return out;
}

namespace fs {
	bool fileExists(const char *filename) {
		return GetFileAttributesA(filename) != INVALID_FILE_ATTRIBUTES;
	}

	std::unique_ptr<uint8_t[]> readWhole(const char *filename, size_t &out_size) {
		File fp(filename);
		std::unique_ptr<uint8_t[]> out;

		if (!fp.fp) {
			err("couldn't open file %s", filename);
			return out;
		}

		fseek(fp, 0, SEEK_END);
		long len = ftell(fp);
		fseek(fp, 0, SEEK_SET);

		out = std::make_unique<uint8_t[]>(len);
		out_size = fread(&out[0], 1, len, fp);

		if (out_size != len) {
			out_size = 0;
			out.reset();
		}

		return out;
	}

	std::vector<uint8_t> readBytes(const char *filename) {
		return readBytes(File(filename));
	}

	std::vector<uint8_t> readBytes(FILE *fp) {
		return readBuf<std::vector<uint8_t>>(fp);
	}

	std::string readString(const char *filename) {
		return readString(File(filename));
	}

	std::string readString(FILE *fp) {
		return readBuf<std::string>(fp);
	}

} // namespace fs