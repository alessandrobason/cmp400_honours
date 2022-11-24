#pragma once

#include <stdio.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <memory>

namespace fs {
	struct File {
		FILE *fp = nullptr;
		File() = default;
		File(const char *filename) { fp = fopen(filename, "rb"); }
		~File() { if (fp) fclose(fp); }
		operator FILE*() { return fp; }
	};

	std::unique_ptr<uint8_t[]> readWhole(const char *filename, size_t &out_size);
	std::vector<uint8_t> readBytes(const char *filename);
	std::vector<uint8_t> readBytes(FILE *fp);
	std::string readString(const char *filename);
	std::string readString(FILE *fp);
} // namespace fs