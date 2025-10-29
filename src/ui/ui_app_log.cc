#include "./ui_app_log.h"

#include "imgui.h"
#include "ui/ui_util.h"
#include "util/ringbuffer.h"

#include <cstring>
#include <string>

#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

//-----------------------------------------------------------------------------
// [SECTION] Example App: Debug Log
//-----------------------------------------------------------------------------

// Usage:
//  static ExampleAppLog my_log;
//  my_log.AddLog("Hello %d world\n", 123);
//  my_log.Draw("title");
struct UiAppLog {
    ImGuiTextBuffer Buf;
    ImGuiTextFilter Filter;
    ImVector<int> LineOffsets;  // Index to lines offset. We maintain this with AddLog() calls.
    bool AutoScroll;            // Keep scrolling if already at the bottom.

    ui_app_log_t* win;

    UiAppLog() {
        AutoScroll = true;
        Clear();
    }

    void Clear() {
        Buf.clear();
        LineOffsets.clear();
        LineOffsets.push_back(0);
    }

    void AddLog(const char* fmt, ...) IM_FMTARGS(2) {
        int old_size = Buf.size();
        va_list args;
        va_start(args, fmt);
        Buf.appendfv(fmt, args);
        va_end(args);
        for (int new_size = Buf.size(); old_size < new_size; old_size++)
            if (Buf[old_size] == '\n') LineOffsets.push_back(old_size + 1);
    }

    void Draw(const char* title, bool* p_open = NULL) {
        if (!ImGui::Begin(title, p_open)) {
            ImGui::End();
            return;
        }

        // Options menu
        if (ImGui::BeginPopup("Options")) {
            ImGui::Checkbox("Auto-scroll", &AutoScroll);
            ImGui::EndPopup();
        }

        // Main window
        if (ImGui::Button("Options")) ImGui::OpenPopup("Options");
        ImGui::SameLine();
        bool clear = ImGui::Button("Clear");
        ImGui::SameLine();
        bool copy = ImGui::Button("Copy");
        ImGui::SameLine();
        Filter.Draw("Filter", -100.0f);

        ImGui::Separator();

        if (ImGui::BeginChild("scrolling", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar)) {
            if (clear) Clear();
            if (copy) ImGui::LogToClipboard();

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            const char* buf = Buf.begin();
            const char* buf_end = Buf.end();
            if (Filter.IsActive()) {
                // In this example we don't use the clipper when Filter is enabled.
                // This is because we don't have random access to the result of our filter.
                // A real application processing logs with ten of thousands of entries may want to store the result of
                // search/filter.. especially if the filtering function is not trivial (e.g. reg-exp).
                for (int line_no = 0; line_no < LineOffsets.Size; line_no++) {
                    const char* line_start = buf + LineOffsets[line_no];
                    const char* line_end =
                        (line_no + 1 < LineOffsets.Size) ? (buf + LineOffsets[line_no + 1] - 1) : buf_end;
                    if (Filter.PassFilter(line_start, line_end)) ImGui::TextUnformatted(line_start, line_end);
                }
            }
            else {
                // The simplest and easy way to display the entire buffer:
                //   ImGui::TextUnformatted(buf_begin, buf_end);
                // And it'll just work. TextUnformatted() has specialization for large blob of text and will
                // fast-forward to skip non-visible lines. Here we instead demonstrate using the clipper to only process
                // lines that are within the visible area. If you have tens of thousands of items and their processing
                // cost is non-negligible, coarse clipping them on your side is recommended. Using ImGuiListClipper
                // requires
                // - A) random access into your data
                // - B) items all being the  same height,
                // both of which we can handle since we have an array pointing to the beginning of each line of text.
                // When using the filter (in the block of code above) we don't have random access into the data to
                // display anymore, which is why we don't use the clipper. Storing or skimming through the search result
                // would make it possible (and would be recommended if you want to search through tens of thousands of
                // entries).
                ImGuiListClipper clipper;
                clipper.Begin(LineOffsets.Size);
                while (clipper.Step()) {
                    for (int line_no = clipper.DisplayStart; line_no < clipper.DisplayEnd; line_no++) {
                        const char* line_start = buf + LineOffsets[line_no];
                        const char* line_end =
                            (line_no + 1 < LineOffsets.Size) ? (buf + LineOffsets[line_no + 1] - 1) : buf_end;
                        ImGui::TextUnformatted(line_start, line_end);
                    }
                }
                clipper.End();
            }
            ImGui::PopStyleVar();

            // Keep up at the bottom of the scroll region if we were already at the bottom at the beginning of the
            // frame. Using a scrollbar or mouse-wheel will take away from the bottom edge.
            if (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();
        ImGui::End();
    }
};

static UiAppLog app_log;

void ui_app_log_add(uint32_t log_level, uint32_t log_item, const char* log_id, const char* message) {
    const char* log_level_str;
    switch (log_level) {
        case 0: log_level_str = "panic"; break;
        case 1: log_level_str = "error"; break;
        case 2: log_level_str = "warning"; break;
        default: log_level_str = "info"; break;
    }

    app_log.AddLog("[%s][%u] %s() %s\n", log_level_str, log_item, log_id, message);
}

void ui_app_log_init(ui_app_log_t* win, const ui_app_log_desc_t* desc) {
    CHIPS_ASSERT(win && desc);
    CHIPS_ASSERT(desc->title);
    memset(win, 0, sizeof(ui_app_log_t));
    win->title = desc->title;
    win->init_x = (float)desc->x;
    win->init_y = (float)desc->y;
    win->init_w = (float)((desc->w == 0) ? 400 : desc->w);
    win->init_h = (float)((desc->h == 0) ? 256 : desc->h);
    win->open = win->last_open = desc->open;
    win->valid = true;

    app_log.win = win;
}

void ui_app_log_discard(ui_app_log_t* win) {
    CHIPS_ASSERT(win && win->valid);
    win->valid = false;
    app_log.Clear();
}

void ui_app_log_draw(ui_app_log_t* win) {
    CHIPS_ASSERT(win && win->valid && win->title);
    ui_util_handle_window_open_dirty(&win->open, &win->last_open);
    if (!win->open) {
        return;
    }
    ImGui::SetNextWindowPos(ImVec2(win->init_x, win->init_y), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(win->init_w, win->init_h), ImGuiCond_FirstUseEver);

    app_log.Draw(win->title, &win->open);
}

void ui_app_log_save_settings(ui_app_log_t* win, ui_settings_t* settings) {
    CHIPS_ASSERT(win && settings);
    ui_settings_add(settings, win->title, win->open);
}

void ui_app_log_load_settings(ui_app_log_t* win, const ui_settings_t* settings) {
    CHIPS_ASSERT(win && settings);
    win->open = ui_settings_isopen(settings, win->title);
}
