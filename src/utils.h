#pragma once

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>

#include <initializer_list>

#include "timer.h"
#include "d3d11_fwd.h"

// == memory utils ================================================

namespace mem {
	template<typename T>
	void zero(T &value) { memset(&value, 0, sizeof(T)); }

	template<typename T>
	void copy(T &dst, const T &src) { memcpy(&dst, &src, sizeof(T)); }

	template<typename T> struct RemRef       { using Type = T; };
	template<typename T> struct RemRef<T &>  { using Type = T; };
	template<typename T> struct RemRef<T &&> { using Type = T; };
	template<typename T> using RemRefT = typename RemRef<T>::Type;

	template<typename T> struct RemArr       { using Type = T; };
	template<typename T> struct RemArr<T[]>  { using Type = T; };
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

#undef min
#undef max

	template<typename T>
	T min(const T &a, const T &b) { return a < b ? a : b; }
	template<typename T>
	T max(const T &a, const T &b) { return a > b ? a : b; }

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
} // namespace mem

// == vector ======================================================

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
			reallocate(mem::max(cap * 2, n));
		}
	}

	template<typename ...TArgs>
	T &push(TArgs &&...args) {
		reserve(len + 1);
		mem::placementNew<T>(buf + len, mem::move(args)...);
		return buf[len++];
	}

	void fill(const T& value) {
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
		buf[index] = mem::move(buf[--len]);
	}

	void removeSlow(size_t index) {
		if (index >= len) return;
		const size_t arr_end = --len;
		for (size_t i = index; i < arr_end; ++i) {
			buf[i] = mem::move(buf[i + 1]);
		}
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

// == slice implementation ========================================

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

	operator bool() const { return data != nullptr && len > 0; }

	const T *begin() const { return data; }
	const T *end() const { return data + len; }
	const T &front() const { assert(data); return data[0]; }
	const T &back() const { assert(data); return data[len - 1]; }

	const T *data = nullptr;
	size_t len = 0;
};

// == stream utils ================================================

struct StreamOut {
	uint8_t *getData();
	size_t getLen() const;

	void write(const void *data, size_t len);
	template<typename T>
	void write(const T &data) { write(&data, sizeof(T)); }

	arr<uint8_t> buf;
};

struct StreamIn {
	StreamIn() = default;
	StreamIn(const uint8_t *buf, size_t len) : start(buf), cur(buf), len(len) {}

	bool read(void *data, size_t len);
	template<typename T>
	bool read(T &data) { return read(&data, sizeof(T)); }

	const uint8_t *start = nullptr;
	const uint8_t *cur = nullptr;
	size_t len = 0;
};

// == string utils ================================================

namespace str {
	// TCHAR string
	struct tstr {
		tstr() = default;
		tstr(char *cstr, size_t len = 0);
		tstr(const char *cstr, size_t len = 0);
		tstr(wchar_t *wstr, size_t len = 0);
		tstr(const wchar_t *wstr, size_t len = 0);
		~tstr();
		tstr(tstr &&t);
		tstr &operator=(tstr &&t);

		bool operator==(const tstr &t);

		size_t len() const;
		tstr clone() const;

		operator const TCHAR *() const;

		const TCHAR *buf = nullptr;
		bool is_owned = false;
	};

	struct view : public Slice<char> {
		using Super = typename Slice<char>;
		using Super::Super;

		view(const char *cstr, size_t len = 0);

		bool compare(view v) const;
		mem::ptr<char[]> dup();

		bool operator==(view v) const;
		bool operator!=(view v) const;
	};

	bool ncmp(const char *a, size_t alen, const char *b, size_t blen);
	bool cmp(const char *a, const char *b);

	mem::ptr<wchar_t[]> ansiToWide(const char *cstr, size_t len = 0);
	bool ansiToWide(const char *cstr, size_t src_len, wchar_t *buf, size_t dst_len);

	mem::ptr<char[]> wideToAnsi(const wchar_t *wstr, size_t len = 0);
	bool wideToAnsi(const wchar_t *wstr, size_t src_len, char *buf, size_t dst_len);

	mem::ptr<char[]> formatStr(const char *fmt, ...);

	mem::ptr<char[]> dup(const char *cstr, size_t len = 0);

	int toInt(const char *cstr);
	unsigned int toUInt(const char *cstr);
	double toNum(const char *cstr);

	// returns a formatted string, can generate maximum 16 strings at a time before reusing buffers
	const char *format(const char *fmt, ...);
	const char *formatV(const char *fmt, va_list args);
	char *formatBuf(char *src, size_t srclen, const char *fmt, ...);
	char *formatBufv(char *src, size_t srclen, const char *fmt, va_list args);
} // namespace str 

// == file utils ==================================================

namespace file {
	struct MemoryBuf {
		void destroy() { data.destroy(); data = nullptr; size = 0; }
		operator bool() const { return data.get() != nullptr; }
		mem::ptr<uint8_t[]> data = nullptr;
		size_t size = 0;
	};

	struct Watcher {
		struct WatchedFile {
			WatchedFile(mem::ptr<char[]> &&name, void *custom_data);
			mem::ptr<char []> name = nullptr;
			void *custom_data = nullptr;
		};

		struct ChangedFile {
			ChangedFile(size_t index) : index(index) {}
			size_t index;
			OnceClock clock;
		};

		Watcher() = default;
		Watcher(const char *watch_folder);
		Watcher(Watcher &&w);
		~Watcher();
		Watcher &operator=(Watcher &&w);

		bool init(const char *watch_folder);
		void cleanup();
		void watchFile(const char *name, void *custom_data = nullptr);

		void update();
		WatchedFile *getChangedFiles(WatchedFile *previous = nullptr);
		const char *getWatcherDir() const;

	private:
		void tryUpdate();
		void addChanged(size_t index);
		WatchedFile *getNextChanged();
		size_t findChangedFromWatched(WatchedFile *file);

		arr<WatchedFile> watched;
		arr<ChangedFile> changed;
		uint8_t change_buf[1024]{};
		win32_overlapped_t overlapped;
		win32_handle_t handle = nullptr;
		mem::ptr<char[]> dir;
	};

	struct fp {
		fp() = default;
		fp(const char *filename, const char *mode = "rb");
		fp(const fp &f) = delete;
		fp(fp &&f);
		~fp();

		fp &operator=(fp &&f);

		bool open(const char *filename, const char *mode = "rb");
		void close();

		operator bool() const;

		template<typename T>
		bool read(T &data) {
			return read(&data, sizeof(T));
		}

		template<typename T>
		bool write(const T &data) {
			return write(&data, sizeof(T));
		}

		bool read(void *data, size_t len);
		bool write(const void *data, size_t len);

		bool puts(const char *msg);
		bool print(const char *fmt, ...);

		void *fptr = nullptr;
	};

	bool exists(const char *filename);
	MemoryBuf read(const char *filename);
	bool write(const char *filename, const void *data, size_t len);
	mem::ptr<char[]> findFirstAvailable(const char *dir = ".", const char *name_fmt = "name_%d.txt");
	str::view getFilename(const char *path);
	const char *getExtension(const char *path);
	const char *getNameAndExt(const char *path);
} // namespace file 

// == allocators utils ============================================

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
