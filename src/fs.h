#pragma once

#include "common.h"
#include "timer.h"
#include "str.h"
#include "mem.h"
#include "arr.h"

namespace fs {
	struct MemoryBuf {
		void destroy() { data.destroy(); data = nullptr; size = 0; }
		operator bool() const { return data.get() != nullptr; }
		mem::ptr<uint8_t[]> data = nullptr;
		size_t size = 0;
	};

	struct Watcher {
		struct WatchedFile {
			WatchedFile(mem::ptr<char[]> &&name, void *custom_data);
			mem::ptr<char[]> name = nullptr;
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

	struct file {
		file() = default;
		file(const char *filename, const char *mode = "rb");
		file(const file &f) = delete;
		file(file &&f);
		~file();

		file &operator=(file &&f);

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
		StreamIn(const MemoryBuf &buf) : start(buf.data.get()), cur(start), len(buf.size) {}

		bool isFinished() const;

		bool read(void *data, size_t len);
		template<typename T>
		bool read(T &data) { return read(&data, sizeof(T)); }

		void skip();
		void skipUntil(char until);
		void skipWhitespace();

		str::view getView(char delim);

		const uint8_t *start = nullptr;
		const uint8_t *cur = nullptr;
		size_t len = 0;
	};

	bool exists(const char *filename);
	MemoryBuf read(const char *filename);
	bool write(const char *filename, const void *data, size_t len);
	mem::ptr<char[]> findFirstAvailable(const char *dir = ".", const char *name_fmt = "name_%d.txt");
	str::view getFilename(str::view path);
	str::view getDir(str::view path);
	str::view getBaseDir(str::view path);
	const char *getExtension(const char *path);
	const char *getNameAndExt(const char *path);
} // namespace file 