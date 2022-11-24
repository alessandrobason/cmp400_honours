#include "tracelog.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32 
#pragma warning(disable:4996) // _CRT_SECURE_NO_WARNINGS.
#include <Windows.h>
#undef TLOG_NO_COLOURS
#define TLOG_NO_COLOURS
#ifndef TLOG_VS
#define TLOG_WIN32_NO_VS
#endif
#endif

#ifdef TLOG_VS
#ifndef _WIN32
#error "can't use TLOG_VS if not on windows"
#endif
#endif

#ifdef TLOG_NO_COLOURS
#define BLACK   ""
#define RED     ""
#define GREEN   ""
#define YELLOW  ""
#define BLUE    ""
#define MAGENTA ""
#define CYAN    ""
#define WHITE   ""
#define RESET   ""
#define BOLD    ""
#else
#define BLACK   "\033[30m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[22;34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"
#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#endif

#define MAX_TRACELOG_MSG_LENGTH 1024

bool use_newline = true;

#ifdef TLOG_WIN32_NO_VS
static void setLevelColour(int level) {
    WORD attribute = 15;
    switch (level) {
    case LogDebug:   attribute = 1; break;
    case LogInfo:    attribute = 2; break;
    case LogWarning: attribute = 6; break;
    case LogError:   attribute = 4; break;
    case LogFatal:   attribute = 4; break;
    }

    HANDLE hc = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hc, attribute);
}
#endif

void traceLog(int level, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    traceLogVaList(level, fmt, args);
    va_end(args);
}

#ifdef TLOG_MUTEX
#include "cthreads.h"
static cmutex_t g_mtx = 0;
#endif

void traceLogVaList(int level, const char *fmt, va_list args) {
#ifdef TLOG_MUTEX
    if (!g_mtx) g_mtx = mtxInit();
    mtxLock(g_mtx);
#endif

    char buffer[MAX_TRACELOG_MSG_LENGTH];
    memset(buffer, 0, sizeof(buffer));

    const char *beg;
    switch (level) {
    case LogTrace:   beg = BOLD WHITE  "[TRACE]: "   RESET; break;
    case LogDebug:   beg = BOLD BLUE   "[DEBUG]: "   RESET; break;
    case LogInfo:    beg = BOLD GREEN  "[INFO]: "    RESET; break;
    case LogWarning: beg = BOLD YELLOW "[WARNING]: " RESET; break;
    case LogError:   beg = BOLD RED    "[ERROR]: "   RESET; break;
    case LogFatal:   beg = BOLD RED    "[FATAL]: "   RESET; break;
    default:         beg = "";                              break;
    }

    size_t offset = 0;

#ifndef TLOG_WIN32_NO_VS
    offset = strlen(beg);
    strncpy(buffer, beg, sizeof(buffer));
#endif

    vsnprintf(buffer + offset, sizeof(buffer) - offset, fmt, args);

#if defined(TLOG_VS)
    OutputDebugStringA(buffer);
    if (use_newline) OutputDebugStringA("\n");
#elif defined(TLOG_WIN32_NO_VS)
    SetConsoleOutputCP(CP_UTF8);
    setLevelColour(level);
    printf("%s", beg);
    // set back to white
    setLevelColour(LogTrace);
    printf("%s", buffer);
    if (use_newline) puts("");
#else
    printf("%s", buffer);
    if (use_newline) puts("");
#endif

#ifndef TLOG_DONT_EXIT_ON_FATAL
    if (level == LogFatal) exit(1);
#endif

#ifdef TLOG_MUTEX
    mtxUnlock(g_mtx);
#endif
}

void traceUseNewline(bool newline) {
    use_newline = newline;
}

void traceSetColour(colour_e colour) {
#ifdef TLOG_WIN32_NO_VS
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), colour);
#else
    switch (colour) {
    case COL_RESET:   printf(RESET);   break;
    case COL_BLACK:   printf(BLACK);   break;
    case COL_BLUE:    printf(BLUE);    break;
    case COL_GREEN:   printf(GREEN);   break;
    case COL_CYAN:    printf(CYAN);    break;
    case COL_RED:     printf(RED);     break;
    case COL_MAGENTA: printf(MAGENTA); break;
    case COL_YELLOW:  printf(YELLOW);  break;
    }
#endif
}