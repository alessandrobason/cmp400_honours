#pragma once

#include <stdint.h>

struct OnceClock {
	OnceClock();
	bool after(float seconds);

	uint64_t start = 0;
	bool finished = false;
};

