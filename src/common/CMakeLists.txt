# select the sokol-gfx backend depending on platform
if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    add_definitions(-DSOKOL_GLES3)
    set(slang "glsl300es")
elseif(FIPS_ANDROID)
    add_definitions(-DSOKOL_GLES3)
    set(slang "glsl300es")
elseif(FIPS_WINDOWS)
    add_definitions(-DSOKOL_D3D11)
    set(slang "hlsl4")
elseif(FIPS_OSX)
    add_definitions(-DSOKOL_METAL)
    if (FIPS_IOS)
        set(slang "metal_ios")
    else()
        set(slang "metal_macos")
    endif()
else()
    add_definitions(-DSOKOL_GLCORE)
    if (FIPS_IOS)
        set(slang "glsl300es")
    else()
        set(slang "glsl410")
    endif()
endif()

if (FIPS_WINDOWS)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

# a lib of common code shared between all emulators
add_library(common STATIC
        common.h
        clock.c clock.h
        fs.c fs.h
        gfx.c gfx.h
        keybuf.c keybuf.h
        prof.c prof.h
        webapi.c webapi.h)

set(SHADER_INPUT ${CMAKE_CURRENT_SOURCE_DIR}/shaders.glsl)
set(SHADER_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shaders.glsl.h)
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
add_dependencies(common compile_shaders)
target_include_directories(common PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_sources(common PRIVATE sokol.c)

target_compile_options(common BEFORE PRIVATE -include ${CMAKE_CURRENT_SOURCE_DIR}/config.h)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(common PRIVATE X11 Xcursor Xi EGL GL m dl asound)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    target_link_libraries(common PRIVATE
        "-s USE_WEBGL2=1"  # Enable WebGL 2
        "-s FULL_ES3=1"    # Use OpenGL ES 3, which supports more features like glClearBufferfv
    )
endif()

# optional UI library (using Dear ImGui)
add_library(ui STATIC
    ui.cc ui.h)
add_dependencies(ui imgui-docking common)

# a separate library with just keybuf (for the ASCII emulators)
add_library(keybuf STATIC
    keybuf.c keybuf.h)
