#pragma once

#include <stdint.h>
#include "tracelog.h"
#include "d3d11_fwd.h"

// Used to run an action only once
struct OnceClock {
	OnceClock();
	bool after(float seconds);
	bool once();

	uint64_t start = 0;
	bool finished = false;
};

// Used to get nanosecond accurate CPU performance
struct CPUClock {
	CPUClock(const char *name = nullptr);
	void setName(const char *name);
	uint64_t getTime();
	double getNanoseconds();
	double getMilliseconds();
	double getMicroseconds();
	double getSeconds();
	void print(LogLevel level = LogLevel::Info);

	uint64_t start = 0;
	char debug_name[64] = {};
};

// Used to get GPU performance
struct GPUClock{
	GPUClock(const char *name = nullptr);
	~GPUClock();
	void setName(const char *name);
	void start();
	void end();
	void print(LogLevel level = LogLevel::Info);
	bool isReady();

	size_t start_timer = 0;
	size_t end_timer = 0;
};

void gpuTimerInit();
void gpuTimerCleanup();
void gpuTimerBeginFrame();
void gpuTimerEndFrame();
void gpuTimerPoll();
