#pragma once

#include <stdint.h>
#include "tracelog.h"

struct OnceClock {
	OnceClock();
	bool after(float seconds);

	uint64_t start = 0;
	bool finished = false;
};

struct PerformanceClock {
	PerformanceClock(const char *name = nullptr);
	uint64_t getTime();
	double getNanoseconds();
	double getMilliseconds();
	double getMicroseconds();
	double getSeconds();
	void print(LogLevel level = LogLevel::Info);

	uint64_t start = 0;
	char debug_name[64] = {};
};