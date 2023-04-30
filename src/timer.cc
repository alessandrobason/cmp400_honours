#include "timer.h"

#include <string.h>
#include <d3d11.h>

#include "system.h"
#include "utils.h"

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

// == GPU TIMER =========================================================================================

struct GpuTimer {
	dxptr<ID3D11Query> query = nullptr;
	char debug_name[64] = {0};
	bool is_valid = true;
	bool should_read = false;
	uint64_t value = 0;
};

static size_t gpuTimerAdd(const char *name);
static void gpuTimerRemove(size_t index);
static GpuTimer *gpuTimerGet(size_t index);
static bool gpuTimerPollOne(size_t index);
static void gpuTimerCapture(size_t index);
static bool gpuTimerIsReady(size_t index);
static bool gpuTimerIsReady(GpuTimer &timer);
static double gpuTimerSec(uint64_t time);
static double gpuTimerMs(uint64_t time);
static double gpuTimerUs(uint64_t time);

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
	start = timerNow();
}

void CPUClock::setName(const char *name) {
	strncpy_s(debug_name, name ? name : "(none)", sizeof(debug_name) - 1);
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
	if (timerToSec(time_passed) >= 1.0) {
		logMessage(level, "[%s] time passed: %.3fsec", name_overload, timerToSec(time_passed));
	}
	else if (timerToMilli(time_passed) >= 1.0) {
		logMessage(level, "[%s] time passed: %.3fms", name_overload, timerToMilli(time_passed));
	}
	else if (timerToMicro(time_passed) >= 1.0) {
		logMessage(level, "[%s] time passed: %.3fus", name_overload, timerToMicro(time_passed));
	}
	else {
		logMessage(level, "[%s] time passed: %.3fns", name_overload, timerToNano(time_passed));
	}
	start = timerNow();
}

// == GPU CLOCK =========================================================================================

GPUClock::GPUClock(const char *name) {
	setName(name);
	start_timer = gpuTimerAdd(name);
	end_timer = gpuTimerAdd(name);
}

GPUClock::~GPUClock() {
	gpuTimerRemove(start_timer);
	gpuTimerRemove(end_timer);
}

void GPUClock::setName(const char *name) {
	name = name ? name : "none";
	if (GpuTimer *start = gpuTimerGet(start_timer)) {
		strncpy_s(start->debug_name, name, sizeof(start->debug_name));
	}
	if (GpuTimer *end = gpuTimerGet(end_timer)) {
		strncpy_s(end->debug_name, name, sizeof(end->debug_name));
	}
}

void GPUClock::start() {
	if (!gpuTimerPollOne(start_timer)) {
		gpuTimerCapture(start_timer);
	}
}

void GPUClock::end() {
	if (!gpuTimerPollOne(end_timer)) {
		gpuTimerCapture(end_timer);
	}
}

void GPUClock::print(LogLevel level) {
	uint64_t start = 0, end = 0;
	const char *name = nullptr;

	if (GpuTimer *timer = gpuTimerGet(start_timer)) {
		start = timer->value;
		name = timer->debug_name;
		timer->should_read = false;
	}
	
	if (GpuTimer *timer = gpuTimerGet(end_timer)) {
		end = timer->value;
		if (!name) name = timer->debug_name;
		timer->should_read = false;
	}

	if (!start || !end && end > start) {
		return;
	}

	uint64_t time_passed = end - start;

	if (gpuTimerSec(time_passed) >= 1.0) {
		logMessage(level, "GPU TIMER [%s] time passed: %.3fsec", name, gpuTimerSec(time_passed));
	}
	else if (gpuTimerMs(time_passed) >= 1.0) {
		logMessage(level, "GPU TIMER [%s] time passed: %.3fms", name, gpuTimerMs(time_passed));
	}
	else {
		logMessage(level, "GPU TIMER [%s] time passed: %.3fus", name, gpuTimerUs(time_passed));
	}
}

bool GPUClock::isReady() {
	gpuTimerPollOne(start_timer);
	gpuTimerPollOne(end_timer);
	bool is_ready = true;
	if (GpuTimer *t = gpuTimerGet(start_timer)) is_ready &= (bool)t->value;
	if (GpuTimer *t = gpuTimerGet(end_timer))   is_ready &= (bool)t->value;
	return is_ready;
}

// == GPU TIMER =========================================================================================

#if 0
struct InternalTimer : public GPUTimer {

};

static arr<InternalTimer *> gpu_list;
static VirtualAllocator gpu_arena;

Handle<GPUTimer> GPUTimer::make(const char *name) {
	InternalTimer *timer = (InternalTimer *)gpu_arena.alloc(sizeof(InternalTimer));
	gpu_list.push(timer);
	return timer;
}

bool GPUTimer::isHandleValid(Handle<GPUTimer> handle) {
	return (InternalTimer *)handle.value != nullptr;
}

GPUTimer *GPUTimer::get(Handle<GPUTimer> handle) {
	return (GPUTimer *)handle.value;
}

void GPUTimer::start() {

}

void GPUTimer::end() {
}
#endif


// == GPU TIMER =========================================================================================

static arr<GpuTimer> gpu_timers;

void gpuTimerInit() {
	GpuTimer disjoint_timer;
	strncpy_s(disjoint_timer.debug_name, "disjoint", sizeof(disjoint_timer.debug_name));
	// set initial value to 1 so we never accidentally divide by zero
	disjoint_timer.value = 1;
	
	D3D11_QUERY_DESC disjoint_desc;
	mem::zero(disjoint_desc);
	disjoint_desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
	HRESULT hr = gfx::device->CreateQuery(&disjoint_desc, &disjoint_timer.query);
	if (FAILED(hr)) {
		fatal("couldn't create disjoint query");
	}

	gpu_timers.push(mem::move(disjoint_timer));
}

void gpuTimerCleanup() {
	gpu_timers.clear();
}

void gpuTimerBeginFrame() {
	if (gpu_timers[0].should_read) {
		gfx::context->Begin(gpu_timers[0].query);
	}
}

void gpuTimerEndFrame() {
	if (gpu_timers[0].should_read) {
		gfx::context->End(gpu_timers[0].query);
	}
}

void gpuTimerPoll() {
	GpuTimer &disjoint = gpu_timers[0];
	disjoint.should_read = false;
	
	if (gpuTimerIsReady(disjoint)) {
		disjoint.should_read = true;
		
		D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint_data;
		HRESULT hr = gfx::context->GetData(disjoint.query, &disjoint_data, sizeof(disjoint_data), 0);
		if (FAILED(hr)) {
			gfx::logD3D11messages();
			return;
		}

		if (disjoint_data.Disjoint) {
			warn("Disjointed");
		}

		disjoint.value = disjoint_data.Frequency;
	}
}

static size_t gpuTimerAdd(const char *name) {
	for (size_t i = 0; i < gpu_timers.size(); ++i) {
		if (!gpu_timers[i].is_valid) {
			return i;
		}
	}

	size_t index = gpu_timers.size();
	GpuTimer new_timer;
	strncpy_s(new_timer.debug_name, name ? name : "(none)", sizeof(new_timer.debug_name) - 1);
	
	D3D11_QUERY_DESC desc;
	mem::zero(desc);
	desc.Query = D3D11_QUERY_TIMESTAMP;
	
	HRESULT hr = gfx::device->CreateQuery(&desc, &new_timer.query);
	if (FAILED(hr)) err("couldn't create start query");
	
	gpu_timers.push(mem::move(new_timer));
	
	return index;
}

static void gpuTimerRemove(size_t index) {
	if (GpuTimer *timer = gpuTimerGet(index)) {
		timer->is_valid = false;
	}
}

static GpuTimer *gpuTimerGet(size_t index) {
	if (index < gpu_timers.size()) {
		return &gpu_timers[index];
	}
	return nullptr;
}

static bool gpuTimerPollOne(size_t index) {
	if (GpuTimer *timer = gpuTimerGet(index)) {
		if (!timer->is_valid || !timer->should_read || !gpuTimerIsReady(index)) {
			timer->value = 0;
			return false;
		}

		HRESULT hr = gfx::context->GetData(timer->query, &timer->value, sizeof(UINT64), 0);
		if (FAILED(hr)) {
			err("couldn't get data from timer");
			gfx::logD3D11messages();
			return false;
		}
		return true;
	}
	return false;
}

static void gpuTimerCapture(size_t index) {
	if (GpuTimer *timer = gpuTimerGet(index)) {
		gfx::context->End(timer->query);
		timer->should_read = true;
	}
}

static bool gpuTimerIsReady(size_t index) {
	if (GpuTimer *timer = gpuTimerGet(index)) {
		return gfx::context->GetData(timer->query, nullptr, 0, 0) != S_FALSE; 
	}
	return false;
}

static bool gpuTimerIsReady(GpuTimer &timer) {
	return gfx::context->GetData(timer.query, nullptr, 0, 0) != S_FALSE;
}

static double gpuTimerSec(uint64_t time) {
	return (double)(time) / (double)gpu_timers[0].value;
}

static double gpuTimerMs(uint64_t time) {
	return (double)(time) / (double)gpu_timers[0].value * 1000.0;
}

static double gpuTimerUs(uint64_t time) {
	return (double)(time) / (double)gpu_timers[0].value * 1000000.0;
}
