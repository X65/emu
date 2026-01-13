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

add_library(speex-resampler STATIC
    speexdsp/libspeexdsp/resample.c
)
target_include_directories(speex-resampler PUBLIC
    speexdsp/include
    speexdsp/include/speex
)
target_compile_definitions(speex-resampler PUBLIC
    OUTSIDE_SPEEX
    RANDOM_PREFIX=x65_
    FLOATING_POINT
)

# SDL3 options (keep the vendor build minimal)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON  CACHE BOOL "" FORCE)
set(SDL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
set(SDL_TESTS          OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES       OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY   OFF CACHE BOOL "" FORCE)
# don't need to show windows
set(SDL_UNIX_CONSOLE_BUILD  ON CACHE BOOL "" FORCE)
set(SDL_VIDEO OFF CACHE BOOL "" FORCE)
set(SDL_AUDIO OFF CACHE BOOL "" FORCE)
add_subdirectory(SDL EXCLUDE_FROM_ALL)
