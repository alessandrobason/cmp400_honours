#pragma once

#define SAFE_RELEASE(p) if (p) { (p)->Release(); (p) = nullptr; }
#define UNUSED(x) ((void)(x))
#define ARRLEN(arr) (sizeof(arr)/sizeof(*(arr)))