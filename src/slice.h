#pragma once

#include <limits.h>
#include <assert.h>
#include <initializer_list>

#include "arr.h"

template<typename T>
struct Slice {
	Slice() = default;
	Slice(const T *data, size_t len) : data(data), len(len) {}
	template<size_t size>
	Slice(const T(&data)[size]) : data(data), len(size) {}
	Slice(std::initializer_list<T> list) : data(list.begin()), len(list.size()) {}
	Slice(const arr<T> &list) : data(list.data()), len(list.size()) {}

	bool empty() const {
		return len == 0;
	}

	size_t byteSize() const {
		return len * sizeof(T);
	}

	Slice sub(size_t start, size_t end = SIZE_MAX) const {
		if (empty() || start >= len) return Slice();
		if (end >= len) end = len;
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

	operator bool() const { return data != nullptr && len > 0; }

	const T *begin() const { return data; }
	const T *end() const { return data + len; }
	const T &front() const { assert(data); return data[0]; }
	const T &back() const { assert(data); return data[len - 1]; }

	const T *data = nullptr;
	size_t len = 0;
};