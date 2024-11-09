add_library(ext STATIC args.c)

add_compile_definitions(-DSOKOL_GLES3)
set(slang "glsl300es")
target_sources(ext PRIVATE
    chips-test/examples/common/common.h
    chips-test/examples/common/clock.c
    chips-test/examples/common/fs.c
    chips-test/examples/common/gfx.c
    chips-test/examples/common/keybuf.c
    chips-test/examples/common/prof.c
    chips-test/examples/common/webapi.c
    chips-test/examples/common/common.h
    chips-test/examples/common/ui.cc
)

set(SHADER_INPUT ${CMAKE_CURRENT_SOURCE_DIR}/chips-test/examples/common/shaders.glsl)
set(SHADER_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/chips-test/examples/common/shaders.glsl.h)
add_custom_command(
    OUTPUT ${SHADER_OUTPUT}
    COMMAND ${SOKOL_SHDC_EXECUTABLE}
            --input ${SHADER_INPUT}
            --output ${SHADER_OUTPUT}
            --format sokol
            --slang ${slang}
    DEPENDS ${SHADER_INPUT}
    COMMENT "Compiling shaders with sokol-shdc"
)
add_custom_target(compile_shaders DEPENDS ${SHADER_OUTPUT})
add_dependencies(ext compile_shaders)
target_include_directories(ext PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/chips-test/examples/common)

target_sources(ext PRIVATE chips-test/examples/common/sokol.c)
target_link_libraries(ext PRIVATE X11 Xcursor Xi EGL GLESv2 m dl asound)
target_compile_options(ext BEFORE PRIVATE -include ${CMAKE_CURRENT_SOURCE_DIR}/config.h)

add_library(imgui STATIC
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
