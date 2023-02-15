#include "tracelog.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <sokol_time.h>
#include <imgui.h>

#include "system.h"
#include "utils.h"

static const char *level_strings[(int)LogLevel::Count] = {
    "[NONE] ",
    "[DEBUG]",
    "[INFO] ",
    "[WARN] ",
    "[ERROR]",
    "[FATAL]",
};

ImColor level_colours[(int)LogLevel::Count] = {
    ImColor(  0,   0,   0), // None
    ImColor( 26, 102, 191), // Debug
    ImColor( 30, 149,  96), // Info
    ImColor(230, 179,   0), // Warning
    ImColor(251,  35,  78), // Error
    ImColor(255,   0,   0), // Fatal
};

struct Line {
    LogLevel level = LogLevel::None;
    int offset = 0;
    int minutes = 0;
    int seconds = 0;
    int millis = 0;
};

struct Logger {
    Logger() { SetConsoleOutputCP(CP_UTF8); clear(); if (fp) fclose(fp); }

    void clear();
    void endLine(LogLevel level, int minutes, int seconds, int millis);
    void draw();

    FILE *fp = nullptr;
    ImGuiTextBuffer buf;
    ImVector<Line> lines;
    bool auto_scrool = true;
};

static Logger logger;

void logMessage(LogLevel level, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logMessageV(level, fmt, args);
    va_end(args);
}

void logMessageV(LogLevel level, const char *fmt, va_list vlist) {
    double time_millis = stm_ms(stm_now());
    double time_seconds = time_millis / 1000.0;
    int minutes = (int)(time_seconds / 60.0);
    int seconds = (int)(time_seconds) % 60;
    int millis = (int)(time_millis) % 1000;

    int start = logger.buf.size();
    logger.buf.appendfv(fmt, vlist);
    logger.endLine(level, minutes, seconds, millis);

    // also print to terminal

    HANDLE hc = GetStdHandle(STD_OUTPUT_HANDLE);

    WORD attribute = 15;
    switch (level) {
        case LogLevel::Debug:   SetConsoleTextAttribute(hc, 1); break;
        case LogLevel::Info:    SetConsoleTextAttribute(hc, 2); break;
        case LogLevel::Warning: SetConsoleTextAttribute(hc, 6); break;
        case LogLevel::Error:   SetConsoleTextAttribute(hc, 4); break;
        case LogLevel::Fatal:   SetConsoleTextAttribute(hc, 4); break;
        default:                SetConsoleTextAttribute(hc, 15); 
    }

    printf("%s", level_strings[(int)level]);
    SetConsoleTextAttribute(hc, 6); // yellow
    printf("(%02d:%02d:%04d): ", minutes, seconds, millis);
    SetConsoleTextAttribute(hc, 15); // white
    printf("%s\n", logger.buf.c_str() + start);

    // also print to file cause why not 
    if (!logger.fp) {
        char name[255];
        mem::zero(name);
        int count = 0;
        while (
            file::exists(
                str::formatBuf(name, sizeof(name), "logs/log_%.3d.txt", count)
            )
        ) {
            count++;
        }

        logger.fp = fopen(name, "wb+");
    }

    fputs(level_strings[(int)level], logger.fp);
    fprintf(logger.fp, "(%02d:%02d:%04d): ", minutes, seconds, millis);
    fprintf(logger.fp, "%s\n", logger.buf.c_str() + start);

    if (level == LogLevel::Fatal) {
        str::tstr temp = str::formatV(fmt, vlist);
        MessageBox(win::hwnd, temp, TEXT("FATAL ERROR"), MB_OK);
        exit(1);
    }
}

void drawLogger() {
    logger.draw();
}

// == LOGGER FUNCTIONS ========================================================================================================

void Logger::clear() {
    buf.clear();
    lines.clear();
    //lines.push_back({});
}

void Logger::endLine(LogLevel level, int minutes, int seconds, int millis) {
    lines.push_back({ level, buf.size(), minutes, seconds, millis });
}

void Logger::draw() {
    static bool is_open = true;
    if (!ImGui::Begin("Logger", &is_open)) {
        ImGui::End();
        return;
    }

    static int filter = 0;

    // Options menu
    if (ImGui::BeginPopup("Options")) {
        ImGui::Checkbox("Auto-scroll", &auto_scrool);
        ImGui::Combo("Filter", &filter, "None\0Debug\0Info\0Warning\0Error");
        ImGui::EndPopup();
    }

    // Main window
    if (ImGui::Button("Options"))
        ImGui::OpenPopup("Options");
    ImGui::SameLine();
    bool should_clear = ImGui::Button("Clear");
    ImGui::SameLine();
    bool should_copy = ImGui::Button("Copy");
    ImGui::SameLine();

    ImGui::Separator();

    if (ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar)) {
        if (should_clear) {
            clear();
        }
        if (should_copy) {
            ImGui::LogToClipboard();
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        const char *buf_beg = buf.begin();

        if (filter) {
            int last_offset = 0;
            LogLevel filter_lv = (LogLevel)filter;
            for (int line_no = 0; line_no < lines.Size; ++line_no) {
                Line &line = lines[line_no];
                if (line.level != filter_lv) {
                    last_offset = line.offset;
                    continue;
                }
                const char *line_start = buf_beg + last_offset;
                const char *line_end = buf_beg + line.offset;
                last_offset = line.offset;
                ImGui::TextColored(level_colours[(int)line.level], level_strings[(int)line.level]);
                ImGui::SameLine();
                ImGui::TextColored(ImColor(220, 220, 170), "(%02d:%02d:%04d): ", line.minutes, line.seconds, line.millis);
                ImGui::SameLine();
                ImGui::TextUnformatted(line_start, line_end);
            }
        }
        else {
            ImGuiListClipper clipper;
            clipper.Begin(lines.Size);
            while (clipper.Step()) {
                int last_offset = clipper.DisplayStart ? lines[clipper.DisplayStart - 1].offset : 0;
                for (int line_no = clipper.DisplayStart; line_no < clipper.DisplayEnd; ++line_no) {
                    Line &line = lines[line_no];
                    const char *line_start = buf_beg + last_offset;
                    const char *line_end = buf_beg + line.offset;
                    last_offset = line.offset;
                    ImGui::TextColored(level_colours[(int)line.level], level_strings[(int)line.level]);
                    ImGui::SameLine();
                    ImGui::TextColored(ImColor(220, 220, 170), "(%02d:%02d:%04d): ", line.minutes, line.seconds, line.millis);
                    ImGui::SameLine();
                    ImGui::TextUnformatted(line_start, line_end);
                }
            }
            clipper.End();
        }

        ImGui::PopStyleVar();

        if (auto_scrool && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
    ImGui::End();
}

#if 0
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sokol_time.h>

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
    case LogTrace:   beg = BOLD WHITE  "[TRACE]"   RESET; break;
    case LogDebug:   beg = BOLD BLUE   "[DEBUG]"   RESET; break;
    case LogInfo:    beg = BOLD GREEN  "[INFO]"    RESET; break;
    case LogWarning: beg = BOLD YELLOW "[WARNING]" RESET; break;
    case LogError:   beg = BOLD RED    "[ERROR]"   RESET; break;
    case LogFatal:   beg = BOLD RED    "[FATAL]"   RESET; break;
    default:         beg = "";                              break;
    }

    double time_millis = stm_ms(stm_now());
    double time_seconds = time_millis / 1000.0;
    int minutes = (int)(time_seconds / 60.0);
    int seconds = (int)(time_seconds) % 60;
    int millis = (int)(time_millis) % 1000;

    size_t offset = 0;

#ifndef TLOG_WIN32_NO_VS
    offset = strlen(beg);
    strncpy(buffer, beg, sizeof(buffer));
    int timelen = vsnprintf(buffer + offset, sizeof(buffer) - offset, "(%02d:%02d:%04d): ", minutes, seconds, millis);
    if (timelen > 0) offset += timelen;
#endif

    vsnprintf(buffer + offset, sizeof(buffer) - offset, fmt, args);

#if defined(TLOG_VS)
    OutputDebugStringA(buffer);
    if (use_newline) OutputDebugStringA("\n");
#elif defined(TLOG_WIN32_NO_VS)
    SetConsoleOutputCP(CP_UTF8);
    setLevelColour(level);
    printf("%s", beg);
    traceSetColour(COL_YELLOW);
    printf("(%02d:%02d:%04d): ", minutes, seconds, millis);
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

#endif
