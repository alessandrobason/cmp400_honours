#pragma once

#include <stddef.h>
#include <limits.h>
#include <assert.h>

#include <initializer_list>

template<typename T>
struct Slice {
	Slice() = default;
	Slice(const T *data, size_t len) : data(data), len(len) {}
	template<size_t size>
	Slice(const T (&data)[size]) : data(data), len(size) {}
	Slice(std::initializer_list<T> list) : data(list.begin()), len(list.size()) {}

	bool empty() const {
		return len == 0;
	}

	size_t byteSize() const {
		return len * sizeof(T);
	}

	Slice sub(size_t start, size_t end = SIZE_MAX) {
		if (empty() || start >= len) return Slice();
		if (end >= len) end = len - 1;
		return Slice(data + start, end - start);
	}
	
	const T &operator[](size_t i) const {
		assert(i < len);
		return data[i];
	}

	bool operator==(const Slice &s) const {
		if (len != s.len) return false;
		for (size_t i = 0; i < len; ++i) {
			if (data[i] != s[i]) {
				return false;
			}
		}
		return true;
	}

	const T *data = nullptr;
	size_t len = 0;
};