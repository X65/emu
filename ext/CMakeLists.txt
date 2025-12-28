include_directories(imgui)

add_library(imgui-docking STATIC
	imgui/imgui.cpp
	imgui/imgui.h
	imgui/imstb_rectpack.h
	imgui/imstb_textedit.h
	imgui/imstb_truetype.h
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_internal.h
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
)

add_library(imgui-toggle STATIC
	imgui_toggle/imgui_offset_rect.h
    imgui_toggle/imgui_toggle.cpp
    imgui_toggle/imgui_toggle.h
    imgui_toggle/imgui_toggle_math.h
    imgui_toggle/imgui_toggle_palette.cpp
    imgui_toggle/imgui_toggle_palette.h
    imgui_toggle/imgui_toggle_presets.cpp
    imgui_toggle/imgui_toggle_presets.h
    imgui_toggle/imgui_toggle_renderer.cpp
    imgui_toggle/imgui_toggle_renderer.h
)
