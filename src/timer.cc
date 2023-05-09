#include "timer.h"

#include <string.h>
#include <d3d11.h>
#include <math.h>

#include "system.h"
#include "str.h"

// == TIMER STUFF =======================================================================================

struct TimerData {
	LARGE_INTEGER freq;
	LARGE_INTEGER start;
} tm_data;

// prevent 64-bit overflow when computing relative timestamp
// see https://gist.github.com/jspohr/3dc4f00033d79ec5bdaf67bc46c813e3
static int64_t timerInt64MulDiv(int64_t value, int64_t numer, int64_t denom) {
	int64_t q = value / denom;
	int64_t r = value % denom;
	return q * numer + r * numer / denom;
}

void timerInit() {
	QueryPerformanceFrequency(&tm_data.freq);
	QueryPerformanceCounter(&tm_data.start);
}

uint64_t timerNow() {
	LARGE_INTEGER timer_now;
	QueryPerformanceCounter(&timer_now);
	const uint64_t now = (uint64_t)timerInt64MulDiv(timer_now.QuadPart - tm_data.start.QuadPart, 1000000000, tm_data.freq.QuadPart);
	return now;
}

uint64_t timerSince(uint64_t ticks) {
	const uint64_t now = timerNow();
	return now > ticks ? now - ticks : 1;
}

uint64_t timerLaptime(uint64_t &last_time) {
	uint64_t dt = 0;
	uint64_t now = timerNow();
	if (last_time != 0) {
		dt = now > last_time ? now - last_time : 1;
	}
	last_time = now;
	return dt;
}

double timerToSec(uint64_t ticks) {
	return (double)ticks / 1000000000.0;
}

double timerToMilli(uint64_t ticks) {
	return (double)ticks / 1000000.0;
}

double timerToMicro(uint64_t ticks) {
	return (double)ticks / 1000.0;
}

double timerToNano(uint64_t ticks) {
	return (double)ticks;
}

const char *timerFormat(uint64_t ticks) {
	if (timerToSec(ticks) >= 60.0) {
		double seconds = timerToSec(ticks);
		double minutes = floor(seconds / 60.0);
		seconds -= minutes * 60.0;
		return str::format("%.0fmin %.2fs", minutes, seconds);
	}
	else if (timerToSec(ticks) >= 1.0) {
		return str::format("%.2fs", timerToSec(ticks));
	}
	else if (timerToMilli(ticks) >= 1.0) {
		return str::format("%.2fms", timerToMilli(ticks));
	}
	else if (timerToMicro(ticks) >= 1.0) {
		return str::format("%.2fus", timerToMicro(ticks));
	}
	else {
		return str::format("%.2fns", timerToNano(ticks));
	}
}

// == GPU TIMER =========================================================================================

static size_t gpuTimerAdd(const char *name);
static void gpuTimerRemove(size_t index);
static void gpuTimerSetName(size_t index, const char *name);
static void gpuTimerTryBegin(size_t index);
static void gpuTimerTryEnd(size_t index);

// == ONCE CLOCK ========================================================================================

bool OnceClock::after(float seconds) {
	if (!start) start = timerNow();
	
	if (finished) {
		return false;
	}

	finished = (float)timerToSec(timerSince(start)) >= seconds;
	return finished;
}

bool OnceClock::once() {
	if (finished) {
		return false;
	}
	finished = true;
	return true;
}

// == INTERVAL CLOCK ====================================================================================

bool IntervalClock::every(float seconds) {
	if (!start) start = timerNow();

	if ((float)timerToSec(timerSince(start)) >= seconds) {
		start = timerNow();
		return true;
	}
	
	return false;
}

// == CPU CLOCK =========================================================================================

CPUClock::CPUClock(const char *name) {
	setName(name);
	begin();
}

void CPUClock::setName(const char *name) {
	strncpy_s(debug_name, name ? name : "(none)", sizeof(debug_name) - 1);
}

void CPUClock::begin() {
	start = timerNow();
}

uint64_t CPUClock::getTime() {
	return timerSince(start);
}

double CPUClock::getNanoseconds() {
	return timerToNano(timerSince(start));
}

double CPUClock::getMilliseconds() {
	return timerToMilli(timerSince(start));
}

double CPUClock::getMicroseconds() {
	return timerToMicro(timerSince(start));
}

double CPUClock::getSeconds() {
	return timerToSec(timerSince(start));
}

void CPUClock::print(LogLevel level) {
	print(debug_name, level);
}

void CPUClock::print(const char *name_overload, LogLevel level) {
	uint64_t time_passed = timerSince(start);
	logMessage(level, "[%s] time taken: %s", name_overload, timerFormat(time_passed));
	start = timerNow();
}

// == GPU CLOCK =========================================================================================

GPUClock::GPUClock(const char *name) {
	timer_handle = gpuTimerAdd(name ? name : "none");
}

GPUClock::~GPUClock() {
	gpuTimerRemove(timer_handle);
}

void GPUClock::setName(const char *name) {
	gpuTimerSetName(timer_handle, name ? name : "none");
}

void GPUClock::start() {
	gpuTimerTryBegin(timer_handle);
}

void GPUClock::end() {
	gpuTimerTryEnd(timer_handle);
}

// == GPU TIMER =========================================================================================

struct GPUTimer {
	dxptr<ID3D11Query> query;
	uint64_t value;
	bool can_be_read = false;
};

struct GPUTimerPair {
	char debug_name[16];
	GPUTimer start;
	GPUTimer end;
	bool is_valid = false;
};

static arr<GPUTimerPair> gpu_timers;
static GPUTimer disjoint_timer;
static bool disjoint_can_end = false;

static GPUTimerPair *gpuTimerGetPair(size_t index);
static void gpuTimerTryCapture(GPUTimer &timer);
static void gpuTimerTryRead(GPUTimer &timer);
static void gpuTimerTryReadDisjoint();
static bool gpuTimerIsReady(GPUTimer &timer);
static double gpuTimerSec(uint64_t time);
static double gpuTimerMs(uint64_t time);
static double gpuTimerUs(uint64_t time);

void gpuTimerInit() {
	// set initial value to 1 so we never accidentally divide by zero
	disjoint_timer.value = 1;

	D3D11_QUERY_DESC disjoint_desc;
	mem::zero(disjoint_desc);
	disjoint_desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
	HRESULT hr = gfx::device->CreateQuery(&disjoint_desc, &disjoint_timer.query);
	if (FAILED(hr)) {
		fatal("couldn't create disjoint query");
	}
}

void gpuTimerCleanup() {
	gpu_timers.clear();
}

void gpuTimerBeginFrame() {
	if (disjoint_timer.can_be_read) return;
	gfx::context->Begin(disjoint_timer.query);
	disjoint_can_end = true;
}

void gpuTimerEndFrame() {
	if (!disjoint_can_end) return;
	gfx::context->End(disjoint_timer.query);
	disjoint_can_end = false;
	disjoint_timer.can_be_read = true;
}

void gpuTimerPoll() {
	gpuTimerTryReadDisjoint();

	for (GPUTimerPair &pair : gpu_timers) {
		gpuTimerTryRead(pair.start);
		gpuTimerTryRead(pair.end);
		if (pair.start.value && pair.end.value) {
			// print stuff

			if (pair.end.value < pair.start.value) {
				pair.start.value = pair.end.value = 0;
				continue;
			}

			uint64_t time_passed = pair.end.value - pair.start.value;

			if (gpuTimerSec(time_passed) >= 1.0) {
				logMessage(LogLevel::Info, "GPU TIMER [%s] time passed: %.3fsec", pair.debug_name, gpuTimerSec(time_passed));
			}
			else if (gpuTimerMs(time_passed) >= 1.0) {
				logMessage(LogLevel::Info, "GPU TIMER [%s] time passed: %.3fms", pair.debug_name, gpuTimerMs(time_passed));
			}
			else {
				logMessage(LogLevel::Info, "GPU TIMER [%s] time passed: %.3fus", pair.debug_name, gpuTimerUs(time_passed));
			}

			pair.start.value = pair.end.value = 0;
		}
	}
}

static size_t gpuTimerAdd(const char *name) {
	for (size_t i = 0; i < gpu_timers.len; ++i) {
		if (!gpu_timers[i].is_valid) {
			gpu_timers[i].is_valid = true;
			return i;
		}
	}

	size_t index = gpu_timers.len;

	GPUTimerPair new_pair;
	strncpy_s(new_pair.debug_name, name, sizeof(new_pair.debug_name) - 1);

	D3D11_QUERY_DESC desc;
	mem::zero(desc);
	desc.Query = D3D11_QUERY_TIMESTAMP;

	HRESULT hr = E_FAIL;

	hr = gfx::device->CreateQuery(&desc, &new_pair.start.query);
	if (FAILED(hr)) {
		err("couldn't create start query");
		return SIZE_MAX;
	}

	hr = gfx::device->CreateQuery(&desc, &new_pair.end.query);
	if (FAILED(hr)) {
		err("couldn't create end query");
		return SIZE_MAX;
	}

	gpu_timers.push(mem::move(new_pair));
	return index;
}

static void gpuTimerRemove(size_t index) {
	if (GPUTimerPair *timer = gpuTimerGetPair(index)) {
		timer->is_valid = false;
	}
}

static void gpuTimerSetName(size_t index, const char *name) {
	if (GPUTimerPair *timer = gpuTimerGetPair(index)) {
		strncpy_s(timer->debug_name, name, sizeof(timer->debug_name) - 1);
	}
}

static GPUTimerPair *gpuTimerGetPair(size_t index) {
	if (index >= gpu_timers.len) return nullptr;
	return &gpu_timers[index];
}

static void gpuTimerTryCapture(GPUTimer &timer) {
	if (timer.can_be_read) return;
	gfx::context->End(timer.query);
	timer.can_be_read = true;
}

void gpuTimerTryRead(GPUTimer &timer) {
	if (!timer.can_be_read) return;
	timer.value = 0;
	if (!gpuTimerIsReady(timer)) return;

	HRESULT hr = gfx::context->GetData(timer.query, &timer.value, sizeof(timer.value), 0);

	if (FAILED(hr)) {
		err("couldn't get data from timer");
		gfx::logD3D11messages();
	}

	timer.can_be_read = false;
};

static void gpuTimerTryReadDisjoint() {
	if (!disjoint_timer.can_be_read) return;
	if (!gpuTimerIsReady(disjoint_timer)) return;

	D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint_data;
	HRESULT hr = gfx::context->GetData(disjoint_timer.query, &disjoint_data, sizeof(disjoint_data), 0);

	if (FAILED(hr)) return;

	if (disjoint_data.Disjoint) {
		warn("Disjointed");
	}

	disjoint_timer.value = disjoint_data.Frequency;
	disjoint_timer.can_be_read = false;
}

static bool gpuTimerIsReady(GPUTimer &timer) {
	return gfx::context->GetData(timer.query, nullptr, 0, 0) != S_FALSE;
}

static void gpuTimerTryBegin(size_t index) {
	if (GPUTimerPair *pair = gpuTimerGetPair(index)) {
		gpuTimerTryCapture(pair->start);
	}
}

static void gpuTimerTryEnd(size_t index) {
	if (GPUTimerPair *pair = gpuTimerGetPair(index)) {
		gpuTimerTryCapture(pair->end);
	}
	}

static double gpuTimerSec(uint64_t time) {
	return (double)time / (double)disjoint_timer.value;
}

static double gpuTimerMs(uint64_t time) {
	return (double)time / (double)disjoint_timer.value * 1000.0;
}

static double gpuTimerUs(uint64_t time) {
	return (double)time / (double)disjoint_timer.value * 1000000.0;
}
