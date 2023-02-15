#pragma once

#include <tchar.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include <vector>
#include <memory>
#include <string>

#include "timer.h"
#include "d3d11_fwd.h"

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

		operator const TCHAR *() const;

		const TCHAR *buf = nullptr;
		bool is_owned = false;
	};

	std::unique_ptr<wchar_t[]> ansiToWide(const char *cstr, size_t len = 0);
	bool ansiToWide(const char *cstr, size_t src_len, wchar_t *buf, size_t dst_len);

	std::unique_ptr<char[]> wideToAnsi(const wchar_t *wstr, size_t len = 0);
	bool wideToAnsi(const wchar_t *wstr, size_t src_len, char *buf, size_t dst_len);

	std::unique_ptr<char[]> formatStr(const char *fmt, ...);

	std::unique_ptr<char[]> dup(const char *cstr, size_t len = 0);

	// returns a formatted string, can generate maximum 16 strings at a time before reusing buffers
	const char *format(const char *fmt, ...);
	char *formatBuf(char *src, size_t srclen, const char *fmt, ...);
	char *formatBufv(char *src, size_t srclen, const char *fmt, va_list args);
} // namespace str 

// == file utils ==================================================

namespace file {
	struct MemoryBuf {
		std::unique_ptr<uint8_t[]> data = nullptr;
		size_t size = 0;
	};

	struct Watcher {
		struct WatchedFile {
			WatchedFile(std::unique_ptr<char[]> &&name, void *custom_data);
			std::unique_ptr<char []> name = nullptr;
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

	private:
		void tryUpdate();
		void addChanged(size_t index);
		WatchedFile *getNextChanged();
		size_t findChangedFromWatched(WatchedFile *file);

		std::vector<WatchedFile> watched;
		std::vector<ChangedFile> changed;
		uint8_t change_buf[1024];
		win32_overlapped_t overlapped;
		win32_handle_t handle = nullptr;
	};

	bool exists(const char *filename);
	size_t getSize(FILE *fp);

	MemoryBuf read(const char *filename);
	MemoryBuf read(FILE *fp);
	std::string readString(const char *filename);
	std::string readString(FILE *fp);
} // namespace file 

// == memory utils ================================================

namespace mem {
	template<typename T>
	void zero(T &value) { memset(&value, 0, sizeof(T)); }
	
	template<typename T>
	void copy(T &dst, const T &src) { memcpy(&dst, &src, sizeof(T)); }
} // namespace mem

template<typename T>
struct dxptr {
	dxptr() = default;
	dxptr(T *ptr) : ptr(ptr) {}
	dxptr(dxptr &&other) { *this = std::move(other); }
	dxptr &operator=(dxptr &&other) { std::swap(ptr, other.ptr); return *this; }
	~dxptr() { if (ptr) { ptr->Release(); ptr = nullptr; } }

	T *get() { return ptr; }
	const T *get() const { return ptr; }

	operator bool() const { return ptr != nullptr; }
	T *operator->() { return ptr; }
	const T *operator->() const { return ptr; }

private:
	T *ptr = nullptr;
};