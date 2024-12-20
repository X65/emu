#include "./ui_console.h"

#include "imgui.h"
#include "util/ringbuffer.h"

#include <cstring>
#include <string>

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
    win->init_w = (float)((desc->w == 0) ? 400 : desc->w);
    win->init_h = (float)((desc->h == 0) ? 256 : desc->h);
    win->open = desc->open;
    win->auto_scroll = desc->auto_scroll;
    win->scroll_to_bottom = false;
    win->content = new std::string();
    win->valid = true;
}

void ui_console_discard(ui_console_t* win) {
    CHIPS_ASSERT(win && win->valid);
    win->valid = false;
    static_cast<std::string*>(win->content)->clear();
}

void ui_console_draw(ui_console_t* win) {
    CHIPS_ASSERT(win && win->valid && win->title);
    if (!win->open) {
        return;
    }
    ImGui::SetNextWindowPos(ImVec2(win->init_x, win->init_y), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(win->init_w, win->init_h), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(win->title, &win->open)) {
        // As a specific feature guaranteed by the library, after calling Begin() the last Item represent the title bar.
        // So e.g. IsItemHovered() will return true when hovering the title bar.
        // Here we create a context menu only available from the title bar.
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Close Console")) win->open = false;
            ImGui::EndPopup();
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("Clear")) {
            static_cast<std::string*>(win->content)->clear();
        }
        ImGui::SameLine();
        bool copy_to_clipboard = ImGui::SmallButton("Copy");

        ImGui::Separator();

        // Reserve enough left-over height for 1 separator + 1 input text
        const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
        if (ImGui::BeginChild(
                "ScrollingRegion",
                ImVec2(0, -footer_height_to_reserve),
                ImGuiChildFlags_NavFlattened,
                ImGuiWindowFlags_HorizontalScrollbar)) {
            if (ImGui::BeginPopupContextWindow()) {
                if (ImGui::Selectable("Clear")) {
                    static_cast<std::string*>(win->content)->clear();
                }
                ImGui::EndPopup();
            }

            // Pull characters from tx buffer
            uint8_t data;
            if (rb_get(win->tx, &data)) {
                static_cast<std::string*>(win->content)->append((char*)(&data));
            }

            // Display every line as a separate entry so we can change their color or add custom widgets.
            // If you only want raw text you can use ImGui::TextUnformatted(log.begin(), log.end());
            // NB- if you have thousands of entries this approach may be too inefficient and may require user-side
            // clipping to only process visible items. The clipper will automatically measure the height of your first
            // item and then "seek" to display only items in the visible area. To use the clipper we can replace your
            // standard loop:
            //      for (int i = 0; i < Items.Size; i++)
            //   With:
            //      ImGuiListClipper clipper;
            //      clipper.Begin(Items.Size);
            //      while (clipper.Step())
            //         for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
            // - That your items are evenly spaced (same height)
            // - That you have cheap random access to your elements (you can access them given their index,
            //   without processing all the ones before)
            // You cannot this code as-is if a filter is active because it breaks the 'cheap random-access' property.
            // We would need random-access on the post-filtered list.
            // A typical application wanting coarse clipping and filtering may want to pre-compute an array of indices
            // or offsets of items that passed the filtering test, recomputing this array when user changes the filter,
            // and appending newly elements as they are inserted. This is left as a task to the user until we can manage
            // to improve this example code!
            // If your items are of variable height:
            // - Split them into same height items would be simpler and facilitate random-seeking into your list.
            // - Consider using manual call to IsRectVisible() and skipping extraneous decoration from your items.
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));  // Tighten spacing
            if (copy_to_clipboard) ImGui::LogToClipboard();
            // for (const char* item : Items) {
            //     if (!Filter.PassFilter(item)) continue;

            //     // Normally you would store more information in your item than just a string.
            //     // (e.g. make Items[] an array of structure, store color/type etc.)
            //     ImVec4 color;
            //     bool has_color = false;
            //     if (strstr(item, "[error]")) {
            //         color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
            //         has_color = true;
            //     }
            //     else if (strncmp(item, "# ", 2) == 0) {
            //         color = ImVec4(1.0f, 0.8f, 0.6f, 1.0f);
            //         has_color = true;
            //     }
            //     if (has_color) ImGui::PushStyleColor(ImGuiCol_Text, color);
            //     ImGui::TextUnformatted(item);
            //     if (has_color) ImGui::PopStyleColor();
            // }
            std::string* content = static_cast<std::string*>(win->content);
            ImGui::TextUnformatted(content->begin().base(), content->end().base());
            if (copy_to_clipboard) ImGui::LogFinish();

            // Keep up at the bottom of the scroll region if we were already at the bottom at the beginning of the
            // frame. Using a scrollbar or mouse-wheel will take away from the bottom edge.
            if (win->scroll_to_bottom || (win->auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
                ImGui::SetScrollHereY(1.0f);
            win->scroll_to_bottom = false;

            ImGui::PopStyleVar();
        }
        ImGui::EndChild();
        ImGui::Separator();

        // Command-line
        bool reclaim_focus = false;
        ImGuiInputTextFlags input_text_flags =
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_EscapeClearsAll;
        if (ImGui::InputText(
                "Input",
                win->input_buf,
                IM_ARRAYSIZE(win->input_buf),
                input_text_flags,
                NULL,
                (void*)win)) {
            char* s = win->input_buf;
            while (*win->input_buf) {
                if (!rb_put(win->rx, *win->input_buf)) break;
                memcpy(win->input_buf, win->input_buf + 1, strlen(win->input_buf));
            }
            reclaim_focus = true;
        }

        // Auto-focus on window apparition
        ImGui::SetItemDefaultFocus();
        if (reclaim_focus) ImGui::SetKeyboardFocusHere(-1);  // Auto focus previous widget
    }
    ImGui::End();
}
