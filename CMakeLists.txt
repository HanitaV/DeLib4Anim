cmake_minimum_required(VERSION 3.25)
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()
project(stem_engine VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(STEM_BUILD_CLI "Build native CLI executable" ON)
option(STEM_BUILD_TESTS "Build native C++ tests" ON)
option(STEM_BUILD_PYTHON "Build pybind11 Python extension module" OFF)
option(STEM_ENABLE_SKIA "Link external Skia Ganesh OpenGL library when SKIA_ROOT is provided" OFF)

# JetBrains/skia-pack Windows Release archives use static CRT (/MT). Match all MSVC targets before FetchContent/add_library.
if(MSVC AND STEM_ENABLE_SKIA AND NOT CMAKE_MSVC_RUNTIME_LIBRARY)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Prefer workspace .venv Python when available so build-time NVRTC PTX compilation works reliably.
if(NOT Python_EXECUTABLE)
    if(EXISTS "${CMAKE_SOURCE_DIR}/.venv/Scripts/python.exe")
        set(Python_EXECUTABLE "${CMAKE_SOURCE_DIR}/.venv/Scripts/python.exe")
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/.venv/bin/python")
        set(Python_EXECUTABLE "${CMAKE_SOURCE_DIR}/.venv/bin/python")
    endif()
endif()
find_package(Python COMPONENTS Interpreter REQUIRED)

include(cmake/Dependencies.cmake)

# Compile CUDA kernel to embedded PTX header using nvcc (if installed) or nvidia-cuda-nvrtc.
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(COLOR_CONVERT_CU "${CMAKE_SOURCE_DIR}/native/interop/color_convert.cu")
set(COLOR_CONVERT_PTX_HPP "${GENERATED_DIR}/color_convert_ptx.hpp")

add_custom_command(
    OUTPUT "${COLOR_CONVERT_PTX_HPP}"
    COMMAND "${Python_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/compile_cuda_ptx.py"
            --input "${COLOR_CONVERT_CU}"
            --output "${COLOR_CONVERT_PTX_HPP}"
            --symbol kColorConvertPtx
            --arch compute_75
    DEPENDS "${COLOR_CONVERT_CU}" "${CMAKE_SOURCE_DIR}/tools/compile_cuda_ptx.py"
    COMMENT "Compiling CUDA color_convert.cu -> PTX header"
    VERBATIM
)

add_library(stem_native_core STATIC
    native/core/scene.cpp
    native/render/gl_loader.cpp
    native/render/gl_context.cpp
    native/render/renderer.cpp
    native/text/font_atlas.cpp
    native/text/svg_path.cpp
    native/text/skia_backend.cpp
    native/interop/cuda_drv.cpp
    native/encode/nvenc_encoder.cpp
    native/export_pipeline.cpp
    native/benchmark.cpp
    native/doctor.cpp
    "${COLOR_CONVERT_PTX_HPP}"
)

set_target_properties(stem_native_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(stem_native_core
    PUBLIC
        "${CMAKE_SOURCE_DIR}"
        "${GENERATED_DIR}"
)

target_link_libraries(stem_native_core
    PUBLIC
        glfw
        ffnvcodec_headers
        stb_truetype
        ffmpeg_libs
)

if(STEM_ENABLE_SKIA)
    target_link_libraries(stem_native_core PUBLIC skia_ganesh)
endif()

if(WIN32)
    target_compile_definitions(stem_native_core PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(stem_native_core PUBLIC opengl32 gdi32 user32 winmm)
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(stem_native_core PUBLIC OpenGL::GL dl pthread)
endif()

if(MSVC)
    target_compile_options(stem_native_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(stem_native_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

function(stem_copy_runtime_dlls target_name)
    if(WIN32 AND FFMPEG_RUNTIME_DLLS)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${FFMPEG_RUNTIME_DLLS}
                "$<TARGET_FILE_DIR:${target_name}>"
            COMMENT "Copying FFmpeg runtime DLLs next to ${target_name}"
        )
    endif()
endfunction()

if(STEM_BUILD_CLI)
    add_executable(stem_engine_cli native/cli_main.cpp)
    target_link_libraries(stem_engine_cli PRIVATE stem_native_core)
    stem_copy_runtime_dlls(stem_engine_cli)
endif()

if(STEM_BUILD_TESTS)
    enable_testing()
    add_executable(stem_native_tests tests/native_tests.cpp)
    target_link_libraries(stem_native_tests PRIVATE stem_native_core)
    stem_copy_runtime_dlls(stem_native_tests)
    add_test(NAME stem_native_tests COMMAND stem_native_tests WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()

if(STEM_BUILD_PYTHON)
    find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)
    find_package(pybind11 CONFIG REQUIRED)
    pybind11_add_module(_stem_native MODULE native/python_bindings.cpp)
    target_link_libraries(_stem_native PRIVATE stem_native_core)
    if(MINGW)
        target_link_options(_stem_native PRIVATE -static-libgcc -static-libstdc++ -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic)
    endif()
    install(TARGETS _stem_native DESTINATION stem_engine)
    if(WIN32 AND FFMPEG_RUNTIME_DLLS)
        install(FILES ${FFMPEG_RUNTIME_DLLS} DESTINATION stem_engine)
    endif()
endif()
