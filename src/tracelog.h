#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Define any of this to turn on the option
 * -> TLOG_NO_COLOURS:          print without using colours
 * -> TLOG_VS:                  print to visual studio console, also turns on TLOG_NO_COLOURS
 * -> TLOG_DONT_EXIT_ON_FATAL:  don't call 'exit(1)' when using LogFatal
 * -> TLOG_DISABLE:             turn off all logging, useful for release builds
 * -> TLOG_MUTEX:               use a mutex on every traceLog call
*/

//#define TLOG_VS

#include <stdbool.h>
#include <stdarg.h>

enum {
    LogAll, LogTrace, LogDebug, LogInfo, LogWarning, LogError, LogFatal
};

typedef enum {
    COL_RESET = 15,
    COL_BLACK = 8,
    COL_BLUE = 9,
    COL_GREEN = 10,
    COL_CYAN = 11,
    COL_RED = 12,
    COL_MAGENTA = 13,
    COL_YELLOW = 14,
    COL_WHITE = 15
} colour_e;

void traceLog(int level, const char *fmt, ...);
void traceLogVaList(int level, const char *fmt, va_list args);
void traceUseNewline(bool use_newline);
void traceSetColour(colour_e colour);

#ifdef TLOG_DISABLE
#define tall(...)  
#define trace(...) 
#define debug(...) 
#define info(...)  
#define warn(...)  
#define err(...)   
#define fatal(...) 
#else
#define tall(...)  traceLog(LogAll, __VA_ARGS__)
#define trace(...) traceLog(LogTrace, __VA_ARGS__)
#define debug(...) traceLog(LogDebug, __VA_ARGS__)
#define info(...)  traceLog(LogInfo, __VA_ARGS__)
#define warn(...)  traceLog(LogWarning, __VA_ARGS__)
#define err(...)   traceLog(LogError, __VA_ARGS__)
#define fatal(...) traceLog(LogFatal, __VA_ARGS__)
#endif

#ifdef __cplusplus
} // extern "C"
#endif
