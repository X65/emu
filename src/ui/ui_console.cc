#include "./ui_console.h"

#include "imgui.h"
#include "ui/ui_util.h"
#include "util/ringbuffer.h"

#include <time.h>

typedef uint64_t absolute_time_t;
static inline absolute_time_t get_absolute_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t us = ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
    return us;
}
static absolute_time_t make_timeout_time_us(uint64_t us) {
    return get_absolute_time() + us;
}
static inline int64_t absolute_time_diff_us(absolute_time_t from, absolute_time_t to) {
    return (int64_t)(to - from);
}
static inline absolute_time_t delayed_by_us(const absolute_time_t t, uint64_t us) {
    absolute_time_t t2;
    uint64_t base = t;
    uint64_t delayed = base + us;
    if ((int64_t)delayed < 0) {
        // absolute_time_t (to allow for signed time deltas) is never greater than INT64_MAX which == at_the_end_of_time
        delayed = INT64_MAX;
    }
    t2 = delayed;
    return t2;
}

extern "C" {
#include "firmware/src/south/term/color.c"
#include "firmware/src/south/term/term.c"
}

void com_in_write_ansi_CPR(int row, int col) {
    char buf[COM_IN_BUF_SIZE];
    snprintf(buf, COM_IN_BUF_SIZE, "\33[%u;%uR", row, col);
    term_out_chars(buf, strlen(buf));
}

#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

void ui_console_init(ui_console_t* win, const ui_console_desc_t* desc) {
    CHIPS_ASSERT(win && desc);
    CHIPS_ASSERT(desc->title);
    memset(win, 0, sizeof(ui_console_t));
    win->title = desc->title;
    win->rx = desc->rx;
    win->tx = desc->tx;
    win->init_x = (float)desc->x;
    win->init_y = (float)desc->y;
    win->init_w = (float)((desc->w == 0) ? 784 : desc->w);
    win->init_h = (float)((desc->h == 0) ? 572 : desc->h);
    win->open = win->last_open = desc->open;
    win->valid = true;

    term_init();
}

void ui_console_discard(ui_console_t* win) {
    CHIPS_ASSERT(win && win->valid);
    win->valid = false;
}

static void ui_console_process_tx(ui_console_t* win) {
    // Pull characters from tx buffer
    char data[RB_BUFFER_SIZE];
    char* data_ptr = data;
    while (rb_get(win->tx, (uint8_t*)data_ptr))
        ++data_ptr;

    term_out_chars(data, data_ptr - data);
}

void ui_console_draw(ui_console_t* win) {
    CHIPS_ASSERT(win && win->valid && win->title);
    ui_util_handle_window_open_dirty(&win->open, &win->last_open);

    ui_console_process_tx(win);

    term_task();

    if (win->open) {
        ImGui::SetNextWindowPos(ImVec2(win->init_x, win->init_y), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(win->init_w, win->init_h), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin(win->title, &win->open)) {
            ImGui::End();
            return;
        }

        // As a specific feature guaranteed by the library, after calling Begin() the last Item represent the title bar.
        // So e.g. IsItemHovered() will return true when hovering the title bar.
        // Here we create a context menu only available from the title bar.
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Close Console")) win->open = false;
            ImGui::EndPopup();
        }

        // TODO: display items starting from the bottom

        if (ImGui::SmallButton("Clear")) {
            term_out_RIS(&term_96);
        }
        ImGui::SameLine();
        bool copy_to_clipboard = ImGui::SmallButton("Copy");

        ImGui::Separator();

        if (ImGui::BeginChild("ScrollingRegion", ImVec2(), 0, ImGuiWindowFlags_HorizontalScrollbar)) {
            if (ImGui::BeginPopupContextWindow()) {
                if (ImGui::Selectable("Clear")) term_out_RIS(&term_96);
                ImGui::EndPopup();
            }

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));  // Tighten spacing
            if (copy_to_clipboard) ImGui::LogToClipboard();
            for (int y = 0; y < term_96.height; ++y) {
                int mem_y = y + term_96.y_offset;
                if (mem_y >= TERM_MAX_HEIGHT) mem_y -= TERM_MAX_HEIGHT;
                term_data_t* term_ptr = term_96.mem + term_96.width * mem_y;

                for (int x = 0; x < term_96.width; ++x) {
                    if (x > 0) ImGui::SameLine(0, 0);

                    char item[2];
                    item[0] = term_ptr->font_code;
                    item[1] = '\0';
                    ImVec2 item_size = ImGui::CalcTextSize(item);

                    ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
                    ImGui::GetWindowDrawList()->AddRectFilled(
                        cursor_pos,
                        ImVec2(cursor_pos.x + item_size.x, cursor_pos.y + item_size.y),
                        term_ptr->bg_color);

                    ImGui::PushStyleColor(ImGuiCol_Text, term_ptr->fg_color);
                    ImGui::TextUnformatted(item);
                    ImGui::PopStyleColor();

                    ++term_ptr;
                }
            }
            if (copy_to_clipboard) ImGui::LogFinish();

            ImGui::PopStyleVar();
        }
        ImGui::EndChild();

        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
            ImGuiIO& io = ImGui::GetIO();
            const bool is_osx = io.ConfigMacOSXBehaviors;
            const bool ignore_char_inputs = (io.KeyCtrl && !io.KeyAlt) || (is_osx && io.KeyCtrl);
            if (io.InputQueueCharacters.Size > 0) {
                if (!ignore_char_inputs)
                    for (int n = 0; n < io.InputQueueCharacters.Size; n++) {
                        char c = (char)io.InputQueueCharacters[n];
                        if (c) rb_put(win->rx, c);
                    }

                // Consume characters
                io.InputQueueCharacters.resize(0);
            }

            if (ImGui::IsKeyPressed(ImGuiKey::ImGuiKey_Enter)) {
                rb_put(win->rx, '\r');  // CR
                rb_put(win->rx, '\n');  // LF
            }
            if (ImGui::IsKeyPressed(ImGuiKey::ImGuiKey_Delete)) {
                rb_put(win->rx, 0x7F);
            }

            // C0
            if (ImGui::IsKeyPressed(ImGuiKey::ImGuiKey_Backspace)) {
                rb_put(win->rx, '\b');
            }
            if (ImGui::IsKeyPressed(ImGuiKey::ImGuiKey_Tab)) {
                rb_put(win->rx, '\t');
            }
            if (ImGui::IsKeyPressed(ImGuiKey::ImGuiKey_Escape)) {
                rb_put(win->rx, 0x1B);
            }
            // CSI
            if (ImGui::IsKeyPressed(ImGuiKey::ImGuiKey_UpArrow)) {
                rb_put(win->rx, 0x1B);  // ESC
                rb_put(win->rx, '[');
                rb_put(win->rx, 'A');
            }
            if (ImGui::IsKeyPressed(ImGuiKey::ImGuiKey_DownArrow)) {
                rb_put(win->rx, 0x1B);  // ESC
                rb_put(win->rx, '[');
                rb_put(win->rx, 'B');
            }
            if (ImGui::IsKeyPressed(ImGuiKey::ImGuiKey_RightArrow)) {
                rb_put(win->rx, 0x1B);  // ESC
                rb_put(win->rx, '[');
                rb_put(win->rx, 'C');
            }
            if (ImGui::IsKeyPressed(ImGuiKey::ImGuiKey_LeftArrow)) {
                rb_put(win->rx, 0x1B);  // ESC
                rb_put(win->rx, '[');
                rb_put(win->rx, 'D');
            }
        }

        ImGui::End();
    }
}

void ui_console_save_settings(ui_console_t* win, ui_settings_t* settings) {
    CHIPS_ASSERT(win && settings);
    ui_settings_add(settings, win->title, win->open);
}

void ui_console_load_settings(ui_console_t* win, const ui_settings_t* settings) {
    CHIPS_ASSERT(win && settings);
    win->open = ui_settings_isopen(settings, win->title);
}
