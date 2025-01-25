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

include(ExternalProject)
ExternalProject_Add(os-816
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/ext/os-816
    INSTALL_DIR ${CMAKE_BINARY_DIR}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
)
