#include "timer.h"

#include <sokol_time.h>

OnceClock::OnceClock() {
	start = stm_now();
}

bool OnceClock::after(float seconds) {
	if (finished) {
		return false;
	}

	finished = stm_sec(stm_now() - start) >= seconds;
	return finished;
}