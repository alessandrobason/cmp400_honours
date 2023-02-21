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