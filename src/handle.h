#pragma once

#include "tracelog.h"
#include "common.h"

template<typename T>
struct Handle {
	Handle() = default;
	Handle(size_t) = delete;
	Handle(T *ptr) : ptr(ptr) {}

	T *get() { return ptr; }
	T *getChecked() { if (!ptr) fatal("failed to get pointer from handle"); return ptr; }
	T *operator->() { return getChecked(); }

	const T *get() const { return ptr; }
	const T *getChecked() const { if (!ptr) fatal("failed to get pointer from handle"); return ptr; }
	const T *operator->() const { return getChecked(); }

	bool operator==(const Handle &h) const { return ptr == h.ptr; }
	bool operator!=(const Handle &h) const { return ptr != h.ptr; }
	operator bool() const { return ptr != nullptr; }

	T *ptr = nullptr;
};

#if 0
template<typename T>
struct Handle {
	Handle() = default;
	Handle(size_t value) : value(value) {}
	Handle(T *ptr) : value((size_t)ptr) {}

	T *get() const { return T::get(*this); }

	T *getChecked() const {
		T *ptr = get();
		if (!ptr) fatal("failed to get pointer from handle");
		return ptr;
	}

	void destroy() const { T::destroy(*this); }

	bool isValid() const { return T::isHandleValid(*this); }

	T *operator->() const { return getChecked(); }

	bool operator==(const Handle &h) const { return value == h.value; }
	bool operator!=(const Handle &h) const { return value != h.value; }
	operator bool() const { return isValid(); }

	size_t value = 0;
};
#endif