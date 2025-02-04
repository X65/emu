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

add_library(esfmu STATIC
    ESFMu/esfm.h
    ESFMu/esfm.c
    ESFMu/esfm_registers.c
)

