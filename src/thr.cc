#include "thr.h"

#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "mem.h"

namespace thr {
	Mutex::Mutex() {
		internal = malloc(sizeof(CRITICAL_SECTION));
		if (internal) {
			InitializeCriticalSection((CRITICAL_SECTION *)internal);
		}
	}

	Mutex::~Mutex() {
		if (internal) {
			DeleteCriticalSection((CRITICAL_SECTION *)internal);
		}
		free(internal);
	}

	Mutex::Mutex(Mutex &&m) {
		*this = mem::move(m);
	}

	Mutex &Mutex::operator=(Mutex &&m) {
		if (this != &m) {
			mem::swap(internal, m.internal);
		}
		return *this;
	}

	bool Mutex::isValid() {
		return internal != nullptr;
	}

	void Mutex::lock() {
		if (internal) {
			EnterCriticalSection((CRITICAL_SECTION *)internal);
		}
	}

	bool Mutex::tryLock() {
		if (internal) {
			return TryEnterCriticalSection((CRITICAL_SECTION *)internal);
		}
		return false;
	}

	void Mutex::unlock() {
		if (internal) {
			LeaveCriticalSection((CRITICAL_SECTION *)internal);
		}
	}
} // namespace thr