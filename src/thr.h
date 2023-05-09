#pragma once

#include "mem.h"

namespace thr {
	struct Mutex {
		Mutex();
		~Mutex();
		Mutex(Mutex &&);
		Mutex &operator=(Mutex &&m);
		bool isValid();
		void lock();
		bool tryLock();
		void unlock();
		void *internal = nullptr;
	};

	template<typename T>
	struct Promise {
		Promise() = default;
		Promise(const T &value) : value(value), finished(true) {}
		Promise(T &&value) : value(mem::move(value)), finished(true) {}
		Promise(Promise &&p) { *this = mem::move(p); }
		Promise &operator=(Promise &&p) {
			if (this != &p) {
				mtx.lock();
				p.mtx.lock();
				mem::swap(finished, p.finished);
				mem::swap(value, p.value);
				mem::swap(mtx, p.mtx);
				p.mtx.unlock();
				mtx.unlock();
			}
			return *this;
		}

		void reset() {
			mtx.lock();
			finished = false;
			mtx.unlock();
		}

		void set(const T &v) {
			mtx.lock();
			value = v;
			finished = true;
			mtx.unlock();
		}

		void set(T &&v) {
			mtx.lock();
			value = mem::move(v);
			finished = true;
			mtx.unlock();
		}

		bool isFinished() {
			bool result = false;

			if (mtx.tryLock()) {
				result = finished;
				mtx.unlock();
			}

			return result;
		}

		void join() {
			while (!isFinished()) {}
		}

		T value;

	private:
		bool finished = false;
		Mutex mtx;
	};
} // namespace thr