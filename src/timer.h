#pragma once

#include "common.h"
#include "tracelog.h"
#include "d3d11_fwd.h"
#include "handle.h"

void timerInit();
uint64_t timerNow();
uint64_t timerSince(uint64_t ticks);
uint64_t timerLaptime(uint64_t &last_time);
double timerToSec(uint64_t ticks);
double timerToMilli(uint64_t ticks);
double timerToMicro(uint64_t ticks);
double timerToNano(uint64_t ticks);
const char *timerFormat(uint64_t ticks);

void gpuTimerInit();
void gpuTimerCleanup();
void gpuTimerBeginFrame();
void gpuTimerEndFrame();
void gpuTimerPoll();

// Used to run an action only once
struct OnceClock {
	bool after(float seconds);
	bool once();

	uint64_t start = 0;
	bool finished = false;
};

// Used to run a task multiple times at intervals
struct IntervalClock {
	bool every(float seconds);

	uint64_t start = 0;
};

// Used to get nanosecond accurate CPU performance
struct CPUClock {
	CPUClock(const char *name = nullptr);
	void setName(const char *name);
	void begin();
	uint64_t getTime();
	double getNanoseconds();
	double getMilliseconds();
	double getMicroseconds();
	double getSeconds();
	void print(LogLevel level = LogLevel::Info);
	void print(const char *name_overload, LogLevel level = LogLevel::Info);

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

	size_t timer_handle = 0;
};
