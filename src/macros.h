#pragma once

#define SAFE_RELEASE(p) if (p) { (p)->Release(); (p) = nullptr; }
#define SAFE_CLOSE(h) if ((h) && (h) != INVALID_HANDLE_VALUE) { CloseHandle(h); (h) = nullptr; }
#define UNUSED(x) ((void)(x))
#define ARRLEN(arr) (sizeof(arr)/sizeof(*(arr)))