#pragma once

// Right now handles are not stable, this is because underneath it is just a vector, which means
// that the pointer is not stable.
// To fix this, it could use an arena allocator.

#include "tracelog.h"

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
	bool isValid() const { return T::isHandleValid(*this); }

	T *operator->() const { return getChecked(); }

	bool operator==(const Handle &h) const { return value == h.value; }
	bool operator!=(const Handle &h) const { return value != h.value; }
	operator bool() const { return isValid(); }

	size_t value = 0;
};
