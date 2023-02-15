#include "utils.h"

#include <assert.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "tracelog.h"
#include "macros.h"

// == string utils ================================================
namespace str {
	std::unique_ptr<wchar_t[]> ansiToWide(const char *cstr, size_t len) {
		if (len == 0) len = strlen(cstr);
		// MultiByteToWideChar returns length INCLUDING null-terminating character
		int result_len = MultiByteToWideChar(CP_UTF8, 0, cstr, (int)len, NULL, 0);
		std::unique_ptr<wchar_t[]> wstr = std::make_unique<wchar_t[]>(result_len + 1);
		result_len = MultiByteToWideChar(CP_UTF8, 0, cstr, (int)len, wstr.get(), result_len);
		wstr[result_len] = '\0';
		return wstr;
	}

	bool ansiToWide(const char *cstr, size_t src_len, wchar_t *buf, size_t dst_len) {
		if (src_len == 0) src_len = strlen(cstr);
		int result_len = MultiByteToWideChar(CP_UTF8, 0, cstr, (int)src_len, buf, (int)dst_len);
		if (result_len < dst_len) {
			buf[result_len] = '\0';
			return true;
		}
		return false;
	}

	std::unique_ptr<char[]> wideToAnsi(const wchar_t *wstr, size_t len) {
		if (len == 0) len = wcslen(wstr);
		int result_len = WideCharToMultiByte(CP_UTF8, 0, wstr, (int)len, NULL, 0, NULL, NULL);
		std::unique_ptr<char[]> cstr = std::make_unique<char[]>(result_len + 1);
		result_len = WideCharToMultiByte(CP_UTF8, 0, wstr, (int)len, cstr.get(), result_len, NULL, NULL);
		cstr[result_len] = '\0';
		return cstr;
	}

	bool wideToAnsi(const wchar_t *wstr, size_t src_len, char *buf, size_t dst_len) {
		if (src_len == 0) src_len = wcslen(wstr);
		int result_len = WideCharToMultiByte(CP_UTF8, 0, wstr, (int)src_len, buf, (int)dst_len, NULL, NULL);
		if (result_len < dst_len) {
			buf[result_len] = '\0';
			return true;
		}
		return false;
	}

	std::unique_ptr<char[]> formatStr(const char *fmt, ...) {
		std::unique_ptr<char[]> out;
		va_list va, vtemp;
		va_start(va, fmt);

		va_copy(vtemp, va);
		int len = vsnprintf(nullptr, 0, fmt, vtemp);
		va_end(vtemp);
		
		if (len < 0) {
			err("couldn't format string \"%s\"", fmt);
			return out;
		}

		out = std::make_unique<char[]>((size_t)len + 1);
		len = vsnprintf(out.get(), (size_t)len + 1, fmt, va);
		va_end(va);

		return out;
	}

	std::unique_ptr<char[]> dup(const char *cstr, size_t len) {
		if (!cstr) return nullptr;
		if (!len) len = strlen(cstr);
		auto ptr = std::make_unique<char[]>(len + 1);
		memcpy(ptr.get(), cstr, len);
		ptr[len] = '\0';
		return ptr;
	}

	const char *format(const char *fmt, ...) {
		static char static_buf[16][1024];
		static int cur_buf = 0;
		mem::zero(static_buf[cur_buf]);
		va_list va;
		va_start(va, fmt);
		char *str = formatBufv(static_buf[cur_buf], sizeof(static_buf[cur_buf]), fmt, va);
		va_end(va);
		if ((++cur_buf) >= sizeof(static_buf)) cur_buf = 0;
		return str;
	}

	char *formatBuf(char *src, size_t srclen, const char *fmt, ...) {
		va_list va;
		va_start(va, fmt);
		char* str = formatBufv(src, srclen, fmt, va);
		va_end(va);
		return str;
	}

	char *formatBufv(char *src, size_t srclen, const char *fmt, va_list args) {
		int len = vsnprintf(src, srclen, fmt, args);
		if (len >= srclen) {
			len = (int)srclen - 1;
		}
		src[len] = '\0';
		return src;
	}

	tstr::tstr(char *cstr, size_t len) {
		if (!cstr) return;
#ifdef UNICODE
		buf = ansiToWide(cstr, len).release();
		is_owned = true;
#else
		UNUSED(len);
		buf = cstr;
#endif
	}

	tstr::tstr(const char *cstr, size_t len) {
		if (!cstr) return;
#ifdef UNICODE
		is_owned = true;
		buf = ansiToWide(cstr, len).release();
#else
		UNUSED(len);
		//is_owned = should_own;
		//if (should_own) {
		//	if (!len) len = strlen(cstr);
		//	char *str = new char[len + 1];
		//	memcpy(str, cstr, sizeof(char) * len);
		//	str[len] = '\0';
		//	buf = str;
		//}
		//else {
			buf = cstr;
		//}
#endif
	}

	tstr::tstr(wchar_t *wstr, size_t len) {
		if (!wstr) return;
#ifdef UNICODE
		UNUSED(len);
		buf = wstr;
#else
		buf = wideToAnsi(wstr, len).release();
		is_owned = true;
#endif
	}

	tstr::tstr(const wchar_t *wstr, size_t len) {
		if (!wstr) return;
#ifdef UNICODE
		UNUSED(len);
		//is_owned = should_own;
		//if (should_own) {
		//	if (!len) len = wcslen(wstr);
		//	wchar_t *str = new wchar_t[len + 1];
		//	memcpy(str, wstr, sizeof(wchar_t) * len);
		//	str[len] = L'\0';
		//	buf = str;
		//}
		//else {
			buf = wstr;
		//}
#else
		is_owned = true;
		buf = wideToAnsi(wstr, len).release();
#endif
	}

	tstr::~tstr() {
		if (is_owned) {
			delete[] buf;
		}
	}

	tstr::tstr(tstr &&t) {
		*this = std::move(t);
	}

	tstr &tstr::operator=(tstr &&t) {
		if (this != &t) {
			std::swap(buf, t.buf);
			std::swap(is_owned, t.is_owned);
		}
		return *this;
	}

	bool tstr::operator==(const tstr &t) {
		if (!buf || !t.buf) return false;
#ifdef UNICODE
		return wcscmp(buf, t.buf) == 0;
#else
		return strcmp(buf, t.buf) == 0;
#endif
	}

	size_t tstr::len() const {
		if (!buf) return 0;
#ifdef UNICODE
		return wcslen(buf);
#else
		return strlen(buf);
#endif
	}

	tstr::operator const TCHAR *() const {
		return buf;
	}

} // namespace str 

// == file utils ==================================================

namespace file {
	bool exists(const char *filename) {
		return GetFileAttributesA(filename) != INVALID_FILE_ATTRIBUTES;
	}

	size_t getSize(FILE *fp) {
		if (!fp) {
			err("invalid FILE handler passed to getSize");
			return 0;
		}
		fseek(fp, 0, SEEK_END);
		long len = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		return (size_t)len;
	}

	MemoryBuf read(const char *filename) {
		FILE *fp = fopen(filename, "rb");
		if (!fp) {
			err("couldn't open file: %s", filename);
			return {};
		}
		return read(fp);
	}

	MemoryBuf read(FILE *fp) {
		if (!fp) {
			err("invalid FILE handler passed to read");
			return {};
		}

		MemoryBuf out;

		out.size = getSize(fp);
		out.data = std::make_unique<uint8_t[]>(out.size);
		size_t read = fread(out.data.get(), 1, out.size, fp);

		assert(read == out.size);
		if (read != out.size) {
			out = {};
		}

		return out;
	}

	std::string readString(const char *filename) {
		FILE *fp = fopen(filename, "rb");
		if (!fp) {
			err("couldn't open file: %s", filename);
			return {};
		}
		return readString(fp);
	}

	std::string readString(FILE *fp) {
		if (!fp) {
			err("invalid FILE handler passed to readString");
			return {};
		}

		std::string out;
		out.resize(getSize(fp));
		size_t read = fread(&out[0], 1, out.size(), fp);

		assert(read == out.size());
		if (read != out.size()) {
			out.clear();
		}

		return out;
	}

	Watcher::Watcher(const char *watch_folder) {
		init(watch_folder);
	}

	Watcher::Watcher(Watcher &&w) {
		*this = std::move(w);
	}

	Watcher::~Watcher() {
		cleanup();
	}

	Watcher &Watcher::operator=(Watcher &&w) {
		if (this != &w) {
			std::swap(watched, w.watched);
			std::swap(changed, w.changed);
			std::swap(change_buf, w.change_buf);
			std::swap(handle, w.handle);
		}

		return *this;
	}

	bool Watcher::init(const char *watch_folder) {
		if (!watch_folder) return false;

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
			FALSE, // watch subtree
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

	void Watcher::watchFile(const char *name, void *custom_data) {
		// don't add if it is already there
		for (const auto &file : watched) {
			if (strcmp(file.name.get(), name) == 0) {
				if (file.custom_data != custom_data) {
					warn("(watcher) file already added but custom data is different!");
				}
				return;
			}
		}
		watched.emplace_back(str::dup(name), custom_data);
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

					for (size_t i = 0; i < watched.size(); ++i) {
						if (strcmp(watched[i].name.get(), namebuf) == 0) {
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
				// remove it from list by swapping it with the last one
				std::swap(changed[index], changed.back());
				changed.pop_back();
			}
		}

		return getNextChanged();
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

		changed.emplace_back(index);
	}

	Watcher::WatchedFile *Watcher::getNextChanged() {
		for (const auto &file : changed) {
			if (file.clock.finished) {
				return &watched[file.index];
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

	Watcher::WatchedFile::WatchedFile(std::unique_ptr<char[]> &&new_name, void *udata) {
		name = std::move(new_name);
		custom_data = udata;
	}

} // namespace file 