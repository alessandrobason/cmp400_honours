#pragma once

#include <string.h>

#include "common.h"

namespace mem {
	template<typename T>
	void zero(T &value) { memset(&value, 0, sizeof(T)); }

	template<typename T>
	void copy(T &dst, const T &src) { memcpy(&dst, &src, sizeof(T)); }

	template<typename T> struct RemRef { using Type = T; };
	template<typename T> struct RemRef<T &> { using Type = T; };
	template<typename T> struct RemRef<T &&> { using Type = T; };
	template<typename T> using RemRefT = typename RemRef<T>::Type;

	template<typename T> struct RemArr { using Type = T; };
	template<typename T> struct RemArr<T[]> { using Type = T; };
	template<typename T> using RemArrT = typename RemArr<T>::Type;

	template<typename T>
	RemRefT<T> &&move(T &&val) { return (RemRefT<T> &&)val; }

	template<typename T>
	void swap(T &a, T &b) {
		RemArrT<T> temp = mem::move(a);
		a = mem::move(b);
		b = mem::move(temp);
	}

	template<typename T, typename ...TArgs>
	void placementNew(void *data, TArgs &&...args) {
		T *temp = (T *)data;
		*temp = T(mem::move(args)...);
	}

	template<typename T>
	struct ptr {
		ptr() = default;
		ptr(T *p) : buf(p) {}
		ptr(ptr &&p) { *this = mem::move(p); }
		ptr &operator=(ptr &&p) { if (buf != p.buf) swap(p); return *this; }
		~ptr() { destroy(); }

		template<typename ...TArgs>
		static ptr make(TArgs &&...args) {
			return new T(args...);
		}

		void swap(ptr &p) { mem::swap(buf, p.buf); }
		void destroy() { delete buf; buf = nullptr; }
		T *release() { T *temp = buf; buf = nullptr; return temp; }

		operator bool() const { return buf != nullptr; }
		T *get() { return buf; }
		T *operator->() { return buf; }

		const T *get() const { return buf; }
		const T *operator->() const { return buf; }

	private:
		T *buf = nullptr;
	};

	template<typename T>
	struct ptr<T[]> {
		ptr() = default;
		ptr(T *p) : buf(p) {}
		ptr(ptr &&p) { *this = mem::move(p); }
		ptr &operator=(ptr &&p) { if (buf != p.buf) swap(p); return *this; }
		~ptr() { destroy(); }

		static ptr make(size_t len) {
			return new T[len];
		}

		void swap(ptr &p) { mem::swap(buf, p.buf); }
		void destroy() { delete buf; buf = nullptr; }
		T *release() { T *temp = buf; buf = nullptr; return temp; }

		operator bool() const { return buf != nullptr; }
		T *get() { return buf; }
		T *operator->() { return buf; }
		T &operator[](size_t index) { assert(buf); return buf[index]; }

		const T *get() const { return buf; }
		const T *operator->() const { return buf; }
		const T &operator[](size_t index) const { assert(buf); return buf[index]; }

	private:
		T *buf = nullptr;
	};

	struct VirtualAllocator {
		VirtualAllocator() = default;
		~VirtualAllocator();
		VirtualAllocator(VirtualAllocator &&v);
		VirtualAllocator &operator=(VirtualAllocator &&v);

		void init();
		void *alloc(size_t size);
		void rewind(size_t size);

		uint8_t *start = nullptr;
		uint8_t *current = nullptr;
		uint8_t *next_page = nullptr;
	};
} // namespace mem