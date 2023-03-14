#include "timer.h"

#include <string.h>
#include <sokol_time.h>
#include <d3d11.h>

#include "system.h"
#include "utils.h"

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
static void gpuTimerCapture(size_t index);
static bool gpuTimerIsReady(size_t index);
static bool gpuTimerIsReady(GpuTimer &timer);
static double gpuTimerSec(uint64_t time);
static double gpuTimerMs(uint64_t time);
static double gpuTimerUs(uint64_t time);

// == ONCE CLOCK ========================================================================================

OnceClock::OnceClock() {
	start = stm_now();
}

bool OnceClock::after(float seconds) {
	if (finished) {
		return false;
	}

	finished = stm_since(start) >= seconds;
	return finished;
}

bool OnceClock::once() {
	if (finished) {
		return false;
	}
	finished = true;
	return true;
}

// == CPU CLOCK =========================================================================================

CPUClock::CPUClock(const char *name) {
	setName(name);
	start = stm_now();
}

void CPUClock::setName(const char *name) {
	strncpy_s(debug_name, name ? name : "(none)", sizeof(debug_name) - 1);
}

uint64_t CPUClock::getTime() {
	return stm_since(start);
}

double CPUClock::getNanoseconds() {
	return stm_ns(stm_since(start));
}

double CPUClock::getMilliseconds() {
	return stm_ms(stm_since(start));
}

double CPUClock::getMicroseconds() {
	return stm_us(stm_since(start));
}

double CPUClock::getSeconds() {
	return stm_sec(stm_since(start));
}

void CPUClock::print(LogLevel level) {
	uint64_t time_passed = stm_since(start);
	if (stm_sec(time_passed) >= 1.0) {
		logMessage(level, "[%s] time passed: %.3fsec", debug_name, stm_sec(time_passed));
	}
	else if (stm_ms(time_passed) >= 1.0) {
		logMessage(level, "[%s] time passed: %.3fms", debug_name, stm_ms(time_passed));
	}
	else if (stm_us(time_passed) >= 1.0) {
		logMessage(level, "[%s] time passed: %.3fus", debug_name, stm_us(time_passed));
	}
	else {
		logMessage(level, "[%s] time passed: %.3fns", debug_name, stm_ns(time_passed));
	}
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
	gpuTimerCapture(start_timer);
}

void GPUClock::end() {
	gpuTimerCapture(end_timer);
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
	bool is_ready = false;
	if (GpuTimer *t = gpuTimerGet(start_timer)) is_ready |= (bool)t->value;
	if (GpuTimer *t = gpuTimerGet(end_timer)) is_ready |= (bool)t->value;
	return is_ready;
}

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

	gpu_timers.emplace_back(mem::move(disjoint_timer));
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
	HRESULT hr;
	
	if (gpuTimerIsReady(disjoint)) {
		disjoint.should_read = true;
		
		D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint_data;
		hr = gfx::context->GetData(disjoint.query, &disjoint_data, sizeof(disjoint_data), 0);
		if (FAILED(hr)) {
			gfx::logD3D11messages();
			return;
		}

		if (disjoint_data.Disjoint) {
			warn("Disjointed");
		}

		disjoint.value = disjoint_data.Frequency;
	}

	for (size_t i = 1; i < gpu_timers.size(); ++i) {
		GpuTimer &timer = gpu_timers[i];
		
		if (!timer.is_valid || !timer.should_read || !gpuTimerIsReady(i)) {
			timer.value = 0;
			continue;
		}

		hr = gfx::context->GetData(timer.query, &timer.value, sizeof(UINT64), 0);
		if (FAILED(hr)) {
			err("couldn't get data from timer");
			gfx::logD3D11messages();
		}
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
	
	gpu_timers.emplace_back(mem::move(new_timer));
	
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
