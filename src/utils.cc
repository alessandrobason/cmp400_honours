#include "utils.h"

#include <stdio.h>
#include <math.h> // HUGE_VAL

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <d3d11.h>

#include "tracelog.h"

void safeRelease(IUnknown *ptr) {
	if (ptr) {
		ptr->Release();
	}
}

// == stream utils ================================================

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

bool StreamIn::read(void *data, size_t datalen) {
	size_t remaining = len - (cur - start);
	if (remaining < datalen) return false;
	memcpy(data, cur, datalen);
	cur += datalen;
	return true;
}

// == string utils ================================================
namespace str {
	view::view(const char *cstr, size_t len)
		: Super(cstr, cstr ? (len ? len : strlen(cstr)) : 0)
	{
	}

	view::view(Super slice) 
		: Super(slice) 
	{
	}

	void view::removePrefix(size_t count) {
		if (count > len) count = len;
		data += count;
		len -= count;
	}

	void view::removeSuffix(size_t count) {
		if (count > len) count = len;
		len -= count;
	}

	bool view::compare(view v) const {
		return str::ncmp(data, len, v.data, v.len);
	}

	mem::ptr<char[]> view::dup() const {
		return str::dup(data, len);
	}

	size_t view::findFirstOf(view values) const {
		for (size_t i = 0; i < len; ++i) {
			for (size_t k = 0; k < values.len; ++k) {
				if (data[i] == values[k]) return i;
			}
		}
		return SIZE_MAX;
	}

	size_t view::findLastOf(view values) const {
		for (ssize_t i = len - 1; i >= 0; --i) {
			for (size_t k = 0; k < values.len; ++k) {
				if (data[i] == values[k]) return i;
			}
		}
		return SIZE_MAX;
	}

	size_t view::findFirstOf(char value) const {
		for (size_t i = 0; i < len; ++i) {
			if (data[i] == value) return i;
		}
		return SIZE_MAX;
	}

	size_t view::findLastOf(char value) const {
		for (ssize_t i = len - 1; i >= 0; --i) {
			if (data[i] == value) return i;
		}
		return SIZE_MAX;
	}

	bool view::operator==(view v) const {
		return compare(v);
	}

	bool view::operator!=(view v) const {
		return !compare(v);
	}

	bool ncmp(const char *a, size_t alen, const char *b, size_t blen) {
		if (!a && !b) return true;
		if (!a || !b) return false;
		if (alen != blen) return false;
		return memcmp(a, b, alen) == 0;
	}

	bool cmp(const char *a, const char *b) {
		if (!a && !b) return true;
		if (!a || !b) return false;
		return strcmp(a, b) == 0;
	}

	mem::ptr<wchar_t[]> ansiToWide(const char *cstr, size_t len) {
		if (len == 0) len = strlen(cstr);
		// MultiByteToWideChar returns length INCLUDING null-terminating character
		int result_len = MultiByteToWideChar(CP_UTF8, 0, cstr, (int)len, NULL, 0);
		mem::ptr<wchar_t[]> wstr = mem::ptr<wchar_t[]>::make(result_len + 1);
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

	mem::ptr<char[]> wideToAnsi(const wchar_t *wstr, size_t len) {
		if (len == 0) len = wcslen(wstr);
		int result_len = WideCharToMultiByte(CP_UTF8, 0, wstr, (int)len, NULL, 0, NULL, NULL);
		mem::ptr<char[]> cstr = mem::ptr<char[]>::make(result_len + 1);
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

	mem::ptr<char[]> formatStr(const char *fmt, ...) {
		mem::ptr<char[]> out;
		va_list va, vtemp;
		va_start(va, fmt);

		va_copy(vtemp, va);
		int len = vsnprintf(nullptr, 0, fmt, vtemp);
		va_end(vtemp);
		
		if (len < 0) {
			err("couldn't format string \"%s\"", fmt);
			return out;
		}

		out = mem::ptr<char[]>::make((size_t)len + 1);
		len = vsnprintf(out.get(), (size_t)len + 1, fmt, va);
		va_end(va);

		return out;
	}

	mem::ptr<char[]> dup(const char *cstr, size_t len) {
		if (!cstr) return nullptr;
		if (!len) len = strlen(cstr);
		auto ptr = mem::ptr<char[]>::make(len + 1);
		memcpy(ptr.get(), cstr, len);
		ptr[len] = '\0';
		return ptr;
	}

	int toInt(const char* cstr) {
		if (!cstr) return 0;
		int out = strtol(cstr, NULL, 0);
		if (out == LONG_MAX || out == LONG_MIN) {
			out = 0;
		}
		return out;
	}

	unsigned int toUInt(const char* cstr) {
		if (!cstr) return 0;
		unsigned int out = strtoul(cstr, NULL, 0);
		if (out == ULONG_MAX) {
			out = 0;
		}
		return out;
	}

	double toNum(const char* cstr) {
		if (!cstr) return 0;
		double out = strtod(cstr, NULL);
		if (out == HUGE_VAL || out == -HUGE_VAL) {
			out = 0;
		}
		return out;
	}

	const char *format(const char *fmt, ...) {
		va_list va;
		va_start(va, fmt);
		const char *str = formatV(fmt, va);
		va_end(va);
		return str;
	}

	const char *formatV(const char *fmt, va_list args) {
		static char static_buf[16][1024];
		static int cur_buf = 0;
		mem::zero(static_buf[cur_buf]);
		va_list vtemp;
		va_copy(vtemp, args);
		char *str = formatBufv(static_buf[cur_buf], sizeof(static_buf[cur_buf]), fmt, vtemp);
		va_end(vtemp);
		if ((++cur_buf) >= ARRLEN(static_buf)) cur_buf = 0;
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
		buf = cstr;
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
		buf = wstr;
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
		*this = mem::move(t);
	}

	tstr &tstr::operator=(tstr &&t) {
		if (this != &t) {
			mem::swap(buf, t.buf);
			mem::swap(is_owned, t.is_owned);
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

	mem::ptr<char[]> tstr::toAnsi() const {
		if (!buf) return nullptr;
#ifdef UNICODE
		return wideToAnsi(buf);
#else
		return dup(buf);
#endif
	}

	mem::ptr<wchar_t[]> tstr::toWide() const {
		if (!buf) return nullptr;
#ifdef UNICODE
		// str::dup for wstr
		size_t l = wcslen(buf);
		auto ptr = mem::ptr<wchar_t[]>::make(l + 1);
		memcpy(ptr.get(), buf, l);
		ptr[l] = '\0';
		return ptr;
#else
		return ansiToWide(buf);
#endif
	}

	tstr tstr::clone() const {
		return tstr(buf);
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
		FILE *fp = fopen(filename, "rb");
		if (!fp) {
			err("couldn't open file (%s) for reading", filename);
			return {};
		}
		return read(fp);
	}

	bool write(const char *filename, const void *data, size_t len) {
		bool result = true;
		FILE *fp = fopen(filename, "wb");
		if (!fp) {
			err("couldn't open file (%s) for writing", filename);
			return false;
		}

		size_t written = fwrite(data, 1, len, fp);
		if (written != len) {
			err("couldn't write everything to file, written (%zu)", written);
			result = false;
		}

		fclose(fp);
		return result;
	}

	mem::ptr<char[]> findFirstAvailable(const char *dir, const char *name_fmt) {
		char fmt[256];
		mem::zero(fmt);
		str::formatBuf(fmt, sizeof(fmt), "%s/%s", dir, name_fmt);

		char name[255];
		mem::zero(name);

		int count = 0;
		while (file::exists(
			str::formatBuf(name, sizeof(name), fmt, count)
		)) {
			++count;
		}
		return str::dup(name);
	}

	str::view getFilename(str::view path) {
		if (path.len == 0) return path;

		size_t slash = path.findLastOf("/\\");
		size_t dot   = path.findLastOf('.');

		if (slash == SIZE_MAX) slash = 0;
		if (dot == SIZE_MAX)   dot = 0;

		return path.sub(slash + 1, dot);
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

	void Watcher::watchFile(const char *filename, void *custom_data) {
		const char *name = file::getNameAndExt(filename);
		// don't add if it is already there
		for (const auto &file : watched) {
			if (strcmp(file.name.get(), name) == 0) {
				if (file.custom_data != custom_data) {
					warn("(watcher) file already added but custom data is different!");
				}
				return;
			}
		}
		watched.push(str::dup(name), custom_data);
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
				mem::swap(changed[index], changed.back());
				changed.pop();
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

	Watcher::WatchedFile::WatchedFile(mem::ptr<char[]> &&new_name, void *udata) {
		name = mem::move(new_name);
		custom_data = udata;
	}

	fp::fp(const char *filename, const char *mode) {
		open(filename, mode);
	}

	fp::fp(fp &&f) {
		*this = mem::move(f);
	}

	fp::~fp() {
		close();
	}

	fp &fp::operator=(fp &&f) {
		if (this != &f) {
			mem::swap(fptr, f.fptr);
		}
		return *this;
	}

	bool fp::open(const char *filename, const char *mode) {
		fptr = fopen(filename, mode);
		return fptr != nullptr;
	}

	void fp::close() {
		if (fptr) {
			fclose((FILE *)fptr);
			fptr = nullptr;
		}
	}

	fp::operator bool() const {
		return fptr != nullptr;
	}

	bool fp::read(void *data, size_t len) {
		return fread(data, 1, len, (FILE *)fptr) == len;
	}

	bool fp::write(const void *data, size_t len) {
		return fwrite(data, 1, len, (FILE *)fptr) == len;
	}

	bool fp::puts(const char *msg) {
		return fputs(msg, (FILE *)fptr) > EOF;
	}

	bool fp::print(const char *fmt, ...) {
		va_list v;
		va_start(v, fmt);
		int written = vfprintf((FILE *)fptr, fmt, v);
		va_end(v);
		return written > EOF;
	}

} // namespace file

// == allocators utils ============================================

static constexpr size_t kb = 1024;
static constexpr size_t mb = kb * 1024;
static constexpr size_t gb = mb * 1024;
static constexpr size_t allocation = 5 * gb;
static constexpr size_t arena_min_size = 1024 * 10;

static DWORD win_page_size = 0;

VirtualAllocator::~VirtualAllocator() {
	if (start) {
		BOOL result = VirtualFree(start, 0, MEM_RELEASE);
		assert(result);
	}
}

VirtualAllocator::VirtualAllocator(VirtualAllocator &&v) {
	*this = mem::move(v);
}

VirtualAllocator &VirtualAllocator::operator=(VirtualAllocator &&v) {
	if (this != &v) {
		mem::swap(start, v.start);
		mem::swap(current, v.current);
		mem::swap(next_page, v.next_page);
	}

	return *this;
}

void VirtualAllocator::init() {
	if (!win_page_size) {
		SYSTEM_INFO sys_info;
		GetSystemInfo(&sys_info);
		win_page_size = sys_info.dwPageSize;
	}

	size_t to_allocate = allocation - allocation % win_page_size;
	assert(to_allocate % win_page_size == 0);

	start = (uint8_t *)VirtualAlloc(nullptr, to_allocate, MEM_RESERVE, PAGE_READWRITE);
	assert(start);
	current = next_page = start;
}

void *VirtualAllocator::alloc(size_t n) {
	if (!start) init();

	if ((current + n) > next_page) {
		size_t new_pages = n / win_page_size;
		if (n % win_page_size) {
			new_pages++;
		}
		VirtualAlloc(next_page, win_page_size * new_pages, MEM_COMMIT, PAGE_READWRITE);
		next_page += win_page_size * new_pages;
		assert(next_page < (start + allocation));
	}

	void *pointer = current;
	memset(pointer, 0, n);
	current += n;
	return pointer;
}

void VirtualAllocator::rewind(size_t size) {
	assert(start);
	assert(size <= (size_t)(current - start));
	current -= size;
}

namespace thr {
	Mutex::Mutex() {
		internal = malloc(sizeof(CRITICAL_SECTION));
		if (internal) {
			InitializeCriticalSection((CRITICAL_SECTION *)internal);
		}
	}

	Mutex::~Mutex() {
		if (internal) {
			DeleteCriticalSection((CRITICAL_SECTION *)internal);
		}
		free(internal);
	}

	bool Mutex::isValid() {
		return internal != nullptr;
	}

	void Mutex::lock() {
		if (internal) {
			EnterCriticalSection((CRITICAL_SECTION *)internal);
		}
	}

	bool Mutex::tryLock() {
		if (internal) {
			return TryEnterCriticalSection((CRITICAL_SECTION *)internal);
		}
		return false;
	}

	void Mutex::unlock() {
		if (internal) {
			LeaveCriticalSection((CRITICAL_SECTION *)internal);
		}
	}

} // namespace thr