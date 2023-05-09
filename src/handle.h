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

	template<typename Q>
	operator Handle<Q>() const { return static_cast<Q *>(ptr); }

	T *ptr = nullptr;
};
