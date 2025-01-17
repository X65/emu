#include "imgui_internal.h"
#include "sokol_app.h"

#include "./args.h"

extern int window_width;
extern int window_height;

// Custom save function
void Settings_WriteAll(ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf) {
    buf->appendf("[%s][MainWindow]\n", handler->TypeName);
    buf->appendf("Size=%d,%d\n", window_width, window_height);
    buf->append("\n");
}

// Custom load function
void* Settings_ReadOpen(ImGuiContext* ctx, ImGuiSettingsHandler* handler, const char* name) {
    if (strcmp(name, "MainWindow") != 0) return nullptr;
    return &window_width;  // Return your structure to read into
}

void Settings_ReadLine(ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* entry, const char* line) {
    if (entry == &window_width) {
        if (sscanf(line, "Size=%d,%d", &window_width, &window_height) == 2) return;
    }
}

extern "C" void settings_register() {
    ImGuiSettingsHandler iniHandler;
    iniHandler.TypeName = "Emu";
    iniHandler.TypeHash = ImHashStr("Emu");
    iniHandler.ReadOpenFn = Settings_ReadOpen;
    iniHandler.ReadLineFn = Settings_ReadLine;
    iniHandler.WriteAllFn = Settings_WriteAll;
    iniHandler.UserData = NULL;
    ImGui::GetCurrentContext()->SettingsHandlers.push_back(iniHandler);
}

extern "C" void settings_load(const char* ini_file) {
    ImGuiContext* prev_ctx = ImGui::GetCurrentContext();
    ImGuiContext* ctx = ImGui::CreateContext();
    ImGui::SetCurrentContext(ctx);
    settings_register();
    ImGui::LoadIniSettingsFromDisk(ini_file);
    ImGui::DestroyContext(ctx);
    ImGui::SetCurrentContext(prev_ctx);
}
