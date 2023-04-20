#pragma once

constexpr size_t invalid_handle = (size_t)-1;

// Right now handles are not stable, this is because underneath it is just a vector, which means
// that the pointer is not stable.
// To fix this, it could use an arena allocator.

template<typename T>
struct Handle {
	Handle() = default;
	Handle(size_t index) : index(index) {}

	T *get() const { return T::get(*this); }
	bool isValid() const { return T::isHandleValid(*this); }

	bool operator==(const Handle &h) const { return index == h.index; }
	bool operator!=(const Handle &h) const { return index != h.index; }
	operator bool() const { return isValid(); }

	size_t index = invalid_handle;
};
