#include "timer.h"

#include <string.h>
#include <sokol_time.h>

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

PerformanceClock::PerformanceClock(const char *name) {
	strncpy_s(debug_name, name ? name : "(none)", sizeof(debug_name) - 1);
	start = stm_now();
}

uint64_t PerformanceClock::getTime() {
	return stm_since(start);
}

double PerformanceClock::getNanoseconds() {
	return stm_ns(stm_since(start));
}

double PerformanceClock::getMilliseconds() {
	return stm_ms(stm_since(start));
}

double PerformanceClock::getMicroseconds() {
	return stm_us(stm_since(start));
}

double PerformanceClock::getSeconds() {
	return stm_sec(stm_since(start));
}

void PerformanceClock::print(LogLevel level) {
	uint64_t time_passed = stm_since(start);
	if (stm_sec(time_passed) >= 1.0) {
		logMessage(level, "[%s] time passed: %fsec", debug_name, stm_sec(time_passed));
	}
	else if (stm_ms(time_passed) >= 1.0) {
		logMessage(level, "[%s] time passed: %fms", debug_name, stm_ms(time_passed));
	}
	else if (stm_us(time_passed) >= 1.0) {
		logMessage(level, "[%s] time passed: %fus", debug_name, stm_us(time_passed));
	}
	else {
		logMessage(level, "[%s] time passed: %fns", debug_name, stm_ns(time_passed));
	}
}
