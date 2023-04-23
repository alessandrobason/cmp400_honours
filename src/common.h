#pragma once

// small stdint/stddef alternative
using int8_t    = signed char;
using int16_t   = short;
using int32_t   = int;
using int64_t   = long long;
using uint8_t   = unsigned char;
using uint16_t  = unsigned short;
using uint32_t  = unsigned int;
using uint64_t  = unsigned long long;
using ssize_t   = int64_t;
using size_t    = uint64_t;
using uintptr_t = size_t;

#define UNUSED(x) ((void)(x))
#define ARRLEN(arr) (sizeof(arr)/sizeof(*(arr)))
