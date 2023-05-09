#include "fs.h"

#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include <ctype.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace fs {
	bool exists(const char *filename) {
		return GetFileAttributesA(filename) != INVALID_FILE_ATTRIBUTES;
	}

	static size_t getSize(FILE *fp) {
		if (!fp) {
			err("invalid FILE handler passed to getSize");
			return 0;
		}
		fseek(fp, 0, SEEK_END);
		long len = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		return (size_t)len;
	}

	static MemoryBuf read(FILE *fp) {
		if (!fp) {
			err("invalid FILE handler passed to read");
			return {};
		}

		MemoryBuf out;

		out.size = getSize(fp);
		out.data = mem::ptr<uint8_t[]>::make(out.size);
		size_t read = fread(out.data.get(), 1, out.size, fp);

		assert(read == out.size);
		if (read != out.size) {
			out = {};
		}

		return out;
	}

	MemoryBuf read(const char *filename) {
		file fp = file(filename, "rb");
		if (!fp) {
			return {};
		}
		return read((FILE *)fp.fptr);
	}

	bool write(const char *filename, const void *data, size_t len) {
		bool result = true;
		file fp = file(filename, "wb");
		if (!fp) {
			return false;
		}

		if (fp.write(data, len)) {
			err("couldn't write everything to file");
			return false;
		}

		return true;
	}

	mem::ptr<char[]> findFirstAvailable(const char *dir, const char *name_fmt) {
		char fmt[256];
		mem::zero(fmt);
		str::formatBuf(fmt, sizeof(fmt), "%s/%s", dir, name_fmt);

		char name[255];
		mem::zero(name);

		int count = 0;
		while (fs::exists(
			str::formatBuf(name, sizeof(name), fmt, count)
		)) {
			++count;
		}
		return str::dup(name);
	}

	str::view getFilename(str::view path) {
		if (path.len == 0) return path;

		size_t slash = path.findLastOf("/\\");
		size_t dot = path.findLastOf('.');

		if (slash == SIZE_MAX) slash = 0;
		if (dot == SIZE_MAX)   dot = 0;

		return path.sub(slash + 1, dot);
	}

	str::view getDir(str::view path) {
		return path.sub(0, path.findLastOf("/\\"));
	}

	str::view getBaseDir(str::view path) {
		return path.sub(0, path.findFirstOf("/\\"));
	}

	const char *getExtension(const char *path) {
		str::view path_view = path;
		if (path_view.len == 0) return path;

		return path_view.sub(path_view.findLastOf('.') + 1).data;
	}

	const char *getNameAndExt(const char *path) {
		str::view path_view = path;
		if (path_view.len == 0) return path;

		return path_view.sub(path_view.findLastOf("/\\") + 1).data;
	}

	Watcher::Watcher(const char *watch_folder) {
		init(watch_folder);
	}

	Watcher::Watcher(Watcher &&w) {
		*this = mem::move(w);
	}

	Watcher::~Watcher() {
		cleanup();
	}

	Watcher &Watcher::operator=(Watcher &&w) {
		if (this != &w) {
			mem::swap(watched, w.watched);
			mem::swap(changed, w.changed);
			mem::copy(change_buf, w.change_buf);
			mem::swap(handle, w.handle);
		}

		return *this;
	}

	bool Watcher::init(const char *watch_folder) {
		if (!watch_folder) return false;

		dir = str::dup(watch_folder);
		str::tstr dir_name = watch_folder;

		HANDLE dir_handle = CreateFile(
			dir_name,
			FILE_LIST_DIRECTORY,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
			NULL
		);

		if (dir_handle == INVALID_HANDLE_VALUE) {
			err("couldn't open %s directory handle", watch_folder);
			return false;
		}

		overlapped.hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

		BOOL success = ReadDirectoryChangesW(
			dir_handle,
			change_buf, sizeof(change_buf),
			TRUE, // watch subtree
			FILE_NOTIFY_CHANGE_LAST_WRITE,
			nullptr, // bytes returned
			(OVERLAPPED *)&overlapped,
			nullptr // completion routine
		);

		if (!success) {
			err("couldn't start watching %s directory", watch_folder);
			return false;
		}

		handle = dir_handle;

		return true;
	}

	void Watcher::cleanup() {
		SAFE_CLOSE(handle);
		SAFE_CLOSE(overlapped.hEvent);

		watched.clear();
	}

	void Watcher::watchFile(const char *filename, void *custom_data) {
		str::view name = filename;
		if (name.startsWith(dir.get())) {
			// remove dir
			name.removePrefix(strlen(dir.get()));
		}
		
		const char *test_name = str::format("%s%s", dir.get(), name.data);
		if (!fs::exists(test_name)) {
			err("trying to watch a file that doesn't exist: %s", test_name);
		}
		
		// don't add if it is already there
		for (const auto &file : watched) {
			if (name == file.name.get()) {
				if (file.custom_data != custom_data) {
					warn("(watcher) file already added but custom data is different!");
				}
				return;
			}
		}
		watched.push(name.dup(), custom_data);
	}

	void Watcher::update() {
		if (!handle || handle == INVALID_HANDLE_VALUE) {
			return;
		}

		tryUpdate();

		DWORD wait_status = WaitForMultipleObjects(1, &overlapped.hEvent, FALSE, 0);

		if (wait_status == WAIT_TIMEOUT) {
			return;
		}
		else if (wait_status != WAIT_OBJECT_0) {
			warn("unhandles wait_status value: %u", wait_status);
			return;
		}

		DWORD bytes_transferred;
		GetOverlappedResult((HANDLE)handle, (OVERLAPPED *)&overlapped, &bytes_transferred, FALSE);

		FILE_NOTIFY_INFORMATION *event = (FILE_NOTIFY_INFORMATION *)change_buf;

		while (true) {
			switch (event->Action) {
			case FILE_ACTION_MODIFIED:
			{
				char namebuf[1024];
				if (!str::wideToAnsi(event->FileName, event->FileNameLength / sizeof(WCHAR), namebuf, sizeof(namebuf))) {
					err("name too long: %S -- %u", event->FileName, event->FileNameLength);
					break;
				}
				str::view name = namebuf;
				for (size_t i = 0; i < name.len; ++i) {
					if (namebuf[i] == '\\') namebuf[i] = '/';
				}

				for (size_t i = 0; i < watched.size(); ++i) {
					if (name == watched[i].name.get()) {
						addChanged(i);
						break;
					}
				}

				break;
			}
			}

			if (event->NextEntryOffset) {
				*((uint8_t **)&event) += event->NextEntryOffset;
			}
			else {
				break;
			}
		}

		BOOL success = ReadDirectoryChangesW(
			(HANDLE)handle,
			change_buf, sizeof(change_buf),
			FALSE,
			FILE_NOTIFY_CHANGE_LAST_WRITE,
			nullptr,
			(OVERLAPPED *)&overlapped,
			nullptr
		);
		if (!success) {
			err("call to ReadDirectoryChangesW failed");
			return;
		}
	}

	Watcher::WatchedFile *Watcher::getChangedFiles(WatchedFile *previous) {
		if (previous) {
			size_t index = findChangedFromWatched(previous);
			if (index < changed.size()) {
				changed.remove(index);
			}
		}

		return getNextChanged();
	}

	const char *Watcher::getWatcherDir() const {
		return dir.get();
	}

	void Watcher::tryUpdate() {
		for (ChangedFile &file : changed) {
			// this updates the clock and keeps track if it is finished
			file.clock.after(0.1f);
		}
	}

	void Watcher::addChanged(size_t index) {
		for (const auto &file : changed) {
			if (file.index == index) {
				// no need to re-add it to the list
				return;
			}
		}

		changed.push(index);
	}

	Watcher::WatchedFile *Watcher::getNextChanged() {
		for (const auto &file : changed) {
			if (file.clock.finished) {
				WatchedFile &f = watched[file.index];
				const char *filename = str::format(
					"%s/%s", dir.get(), f.name.get()
				);
				// can't read file, skip for now
				if (!fs::file(filename)) {
					continue;
				}
				return &f;
			}
		}

		return nullptr;
	}

	size_t Watcher::findChangedFromWatched(WatchedFile *file) {
		// if it is outside the vector's range, return nullptr
		if (file < watched.data() || file >= (watched.data() + watched.size())) {
			err("WatchedFile is outside of watched data: %p - (%p %p)", file, watched.data(), watched.data() + watched.size());
			return SIZE_MAX;
		}
		// get the index
		size_t index = (size_t)(file - watched.data());

		for (size_t i = 0; i < changed.size(); ++i) {
			if (changed[i].index == index) {
				return i;
			}
		}

		warn("file %s has not changed yet", file->name.get());
		return SIZE_MAX;
	}

	Watcher::WatchedFile::WatchedFile(mem::ptr<char[]> &&new_name, void *udata) {
		name = mem::move(new_name);
		custom_data = udata;
	}

	file::file(const char *filename, const char *mode) {
		open(filename, mode);
	}

	file::file(file &&f) {
		*this = mem::move(f);
	}

	file::~file() {
		close();
	}

	file &file::operator=(file &&f) {
		if (this != &f) {
			mem::swap(fptr, f.fptr);
		}
		return *this;
	}

	bool file::open(const char *filename, const char *mode) {
		FILE *fp = nullptr;
		errno_t result = fopen_s(&fp, filename, mode);
		if (result) {
			return false;
		}
		fptr = fp;
		return true;
	}

	void file::close() {
		if (fptr) {
			fclose((FILE *)fptr);
			fptr = nullptr;
		}
	}

	file::operator bool() const {
		return fptr != nullptr;
	}

	bool file::read(void *data, size_t len) {
		return fread(data, 1, len, (FILE *)fptr) == len;
	}

	bool file::write(const void *data, size_t len) {
		return fwrite(data, 1, len, (FILE *)fptr) == len;
	}

	bool file::puts(const char *msg) {
		return fputs(msg, (FILE *)fptr) > EOF;
	}

	bool file::print(const char *fmt, ...) {
		va_list v;
		va_start(v, fmt);
		int written = vfprintf((FILE *)fptr, fmt, v);
		va_end(v);
		return written > EOF;
	}

	uint8_t *StreamOut::getData() {
		return buf.data();
	}

	size_t StreamOut::getLen() const {
		return buf.size();
	}

	void StreamOut::write(const void *data, size_t len) {
		buf.reserve(buf.len + len);
		memcpy(buf.data() + buf.len, data, len);
		buf.len += len;
	}

	bool StreamIn::isFinished() const {
		return (size_t)(cur - start) >= len;
	}

	bool StreamIn::read(void *data, size_t datalen) {
		size_t remaining = len - (cur - start);
		if (remaining < datalen) return false;
		memcpy(data, cur, datalen);
		cur += datalen;
		return true;
	}

	void StreamIn::skip() {
		if (!isFinished()) {
			++cur;
		}
	}

	void StreamIn::skipUntil(char until) {
		const uint8_t *end = start + len;
		for (; cur < end && *cur != (uint8_t)until; ++cur);
	}

	void StreamIn::skipWhitespace() {
		const uint8_t *end = start + len;
		for (; cur < end && isspace(*cur); ++cur);
	}

	str::view StreamIn::getView(char delim) {
		const uint8_t *from = cur;
		skipUntil(delim);
		size_t view_len = cur - from;
		return str::view((const char *)from, view_len);
	}
} // namespace fs