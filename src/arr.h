#pragma once

#include <limits.h>
#include <assert.h>
#include <stdlib.h>

#include "mem.h"
#include "maths.h"

template<typename T>
struct arr {
	using iterator = T *;
	using const_iterator = const T *;

	arr() = default;
	arr(const arr &a) { *this = a; }
	arr(arr &&a) { *this = mem::move(a); }
	arr(std::initializer_list<T> list) {
		reserve(list.size());
		for (auto &&v : list) push(v);
	}
	~arr() { destroy(); }

	void destroy() {
		for (size_t i = 0; i < len; ++i) {
			buf[i].~T();
		}
		free(buf);
		buf = nullptr;
		cap = len = 0;
	}

	void reserve(size_t n) {
		if (cap < n) {
			reallocate(math::max(cap * 2, n));
		}
	}

	template<typename ...TArgs>
	T &push(TArgs &&...args) {
		reserve(len + 1);
		mem::placementNew<T>(buf + len, mem::move(args)...);
		return buf[len++];
	}

	void fill(const T &value) {
		for (size_t i = 0; i < len; ++i) {
			buf[i] = value;
		}
	}

	void reallocate(size_t newcap) {
		if (newcap < cap) {
			newcap = cap * 2;
		}
		T *newbuf = (T *)calloc(1, sizeof(T) * newcap);
		assert(newbuf);
		for (size_t i = 0; i < len; ++i) {
			mem::placementNew<T>(newbuf + i, mem::move(buf[i]));
		}
		free(buf);
		buf = newbuf;
		cap = newcap;
	}

	void clear() {
		for (size_t i = 0; i < len; ++i) {
			buf[i].~T();
		}
		len = 0;
	}

	void pop() {
		if (len) {
			buf[--len].~T();
		}
	}

	void remove(size_t index) {
		if (index >= len) return;
		mem::swap(buf[index], buf[len - 1]);
		pop();
	}

	void removeSlow(size_t index) {
		if (index >= len) return;
		const size_t arr_end = len - 1;
		for (size_t i = index; i < arr_end; ++i) {
			buf[i] = mem::move(buf[i + 1]);
		}
		pop();
	}

	size_t find(const T &value) const {
		for (size_t i = 0; i < len; ++i) {
			if (buf[i] == value) {
				return i;
			}
		}
		return SIZE_MAX;
	}

	bool contains(const T &value) const {
		return find(value) != SIZE_MAX;
	}

	arr &operator=(const arr &a) {
		clear();
		reserve(a.len);
		for (size_t i = 0; i < a.len; ++i) {
			push(a[i]);
		}
		return *this;
	}

	arr &operator=(arr &&a) {
		mem::swap(buf, a.buf);
		mem::swap(len, a.len);
		mem::swap(cap, a.cap);
		return *this;
	}

	bool empty() const { return len == 0; }
	size_t size() const { return len; }
	size_t capacity() const { return cap; }
	T *data() { return buf; }
	T &operator[](size_t index) { assert(index < len); return buf[index]; }
	T *begin() { return buf; }
	T *end() { return buf + len; }
	T &front() { assert(buf); return buf[0]; }
	T &back() { assert(buf); return buf[len - 1]; }

	const T *data() const { return buf; }
	const T &operator[](size_t index) const { assert(index < len); return buf[index]; }
	const T *begin() const { return buf; }
	const T *end() const { return buf + len; }
	const T &front() const { assert(buf); return buf[0]; }
	const T &back() const { assert(buf); return buf[len - 1]; }

	T *buf = nullptr;
	size_t len = 0;
	size_t cap = 0;
};