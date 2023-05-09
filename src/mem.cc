#include "mem.h"

#include <assert.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "common.h"

static constexpr size_t kb = 1024;
static constexpr size_t mb = kb * 1024;
static constexpr size_t gb = mb * 1024;
static constexpr size_t allocation = 5 * gb;
static constexpr size_t arena_min_size = 1024 * 10;

static DWORD win_page_size = 0;

namespace mem {
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
} // namespace mem