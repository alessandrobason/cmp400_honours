#pragma once

#include <stdarg.h>

enum class LogLevel {
    None,
    Debug, 
    Info, 
    Warning, 
    Error, 
    Fatal,
    Count
};

void logMessage(LogLevel level, const char *fmt, ...);
void logMessageV(LogLevel level, const char *fmt, va_list vlist);
void drawLogger();
void logSetOpen(bool is_open);
bool logIsOpen();

#define debug(...) logMessage(LogLevel::Debug,   __VA_ARGS__)
#define info(...)  logMessage(LogLevel::Info,    __VA_ARGS__)
#define warn(...)  logMessage(LogLevel::Warning, __VA_ARGS__)
#define err(...)   logMessage(LogLevel::Error,   __VA_ARGS__)
#define fatal(...) logMessage(LogLevel::Fatal,   __VA_ARGS__)
