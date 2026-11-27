include(FetchContent)

set(STEM_DEP_CACHE "${CMAKE_SOURCE_DIR}/.stem_cache/deps")
file(MAKE_DIRECTORY "${STEM_DEP_CACHE}")

function(stem_fetch_cached_archive out_var filename url sha256_hex)
    set(_dest "${STEM_DEP_CACHE}/${filename}")
    execute_process(
        COMMAND "${Python_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/fetch_deps.py"
                --url "${url}"
                --dest "${_dest}"
                --sha256 "${sha256_hex}"
        RESULT_VARIABLE _res
    )
    if(NOT _res EQUAL 0)
        message(FATAL_ERROR "Failed to fetch and verify ${url}")
    endif()
    set(${out_var} "${_dest}" PARENT_SCOPE)
endfunction()

# 1. GLFW 3.4 (Pinned Tag + SHA256)
stem_fetch_cached_archive(
    STEM_GLFW_ARCHIVE
    "glfw-3.4.tar.gz"
    "https://github.com/glfw/glfw/archive/refs/tags/3.4.tar.gz"
    "c038d34200234d071fae9345bc455e4a8f2f544ab60150765d7704e08f3dac01"
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    glfw
    URL "${STEM_GLFW_ARCHIVE}"
    URL_HASH SHA256=c038d34200234d071fae9345bc455e4a8f2f544ab60150765d7704e08f3dac01
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(glfw)

# 2. FFmpeg nv-codec-headers n13.1.15.0 (Pinned Commit + SHA256)
stem_fetch_cached_archive(
    STEM_FFNVCODEC_ARCHIVE
    "nv-codec-headers-n13.1.15.0.tar.gz"
    "https://github.com/FFmpeg/nv-codec-headers/archive/0a6fba9a2820628b8103464f4c8753ee05838baa.tar.gz"
    "1d2070546de622fd6074a99d4b283e727988b7c3624ef85f97b88962264314d2"
)
FetchContent_Declare(
    ffnvcodec
    URL "${STEM_FFNVCODEC_ARCHIVE}"
    URL_HASH SHA256=1d2070546de622fd6074a99d4b283e727988b7c3624ef85f97b88962264314d2
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(ffnvcodec)
add_library(ffnvcodec_headers INTERFACE)
target_include_directories(ffnvcodec_headers INTERFACE "${ffnvcodec_SOURCE_DIR}/include")

# 3. stb_truetype v1.26 (Pinned Commit + SHA256)
stem_fetch_cached_archive(
    STEM_STB_TRUETYPE_PATH
    "stb_truetype.h"
    "https://raw.githubusercontent.com/nothings/stb/f58f558c120e9b32c217290b80bad1a0729fbb2c/stb_truetype.h"
    "ecd30b05e0dd4fea3a13c26810dd9e1992dc379049482c393d5a19e6b5090aab"
)
get_filename_component(STEM_STB_DIR "${STEM_STB_TRUETYPE_PATH}" DIRECTORY)
add_library(stb_truetype INTERFACE)
target_include_directories(stb_truetype INTERFACE "${STEM_STB_DIR}")

# 4. FFmpeg Shared Development Libraries (avutil, avcodec, avformat, swresample)
if(WIN32 AND NOT FFMPEG_ROOT)
    stem_fetch_cached_archive(
        STEM_FFMPEG_ARCHIVE
        "ffmpeg-n8.1.3-41-ga8029b6528-win64-lgpl-shared-8.1.zip"
        "https://github.com/FFmpeg-Builds/FFmpeg-Builds/releases/download/autobuild-2026-11-26-12-56/ffmpeg-n8.1.3-41-ga8029b6528-win64-lgpl-shared-8.1.zip"
        "372016b1248544b4ffc290ae41c9c57e6441bd0da32068ea18032a25b7b36e08"
    )
    FetchContent_Declare(
        ffmpeg_win64
        URL "${STEM_FFMPEG_ARCHIVE}"
        URL_HASH SHA256=372016b1248544b4ffc290ae41c9c57e6441bd0da32068ea18032a25b7b36e08
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(ffmpeg_win64)
    set(FFMPEG_ROOT "${ffmpeg_win64_SOURCE_DIR}" CACHE PATH "Pinned FFmpeg Win64 LGPL Shared SDK" FORCE)
endif()

if(WIN32)
    find_path(FFMPEG_INCLUDE_DIR libavcodec/avcodec.h HINTS "${FFMPEG_ROOT}/include" REQUIRED)
    set(FFMPEG_BIN_DIR "${FFMPEG_ROOT}/bin")
    set(FFMPEG_LIB_DIR "${FFMPEG_ROOT}/lib")

    add_library(ffmpeg_libs INTERFACE)
    target_include_directories(ffmpeg_libs INTERFACE "${FFMPEG_INCLUDE_DIR}")

    foreach(_fflib avutil avcodec avformat swresample)
        find_library(FFMPEG_${_fflib}_LIB
            NAMES ${_fflib} lib${_fflib}
            HINTS "${FFMPEG_LIB_DIR}"
            NO_DEFAULT_PATH
            REQUIRED
        )
        target_link_libraries(ffmpeg_libs INTERFACE "${FFMPEG_${_fflib}_LIB}")
    endforeach()

    file(GLOB FFMPEG_RUNTIME_DLLS "${FFMPEG_BIN_DIR}/*.dll")
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavutil libavcodec libavformat libswresample)
    add_library(ffmpeg_libs INTERFACE)
    target_link_libraries(ffmpeg_libs INTERFACE PkgConfig::FFMPEG)
    set(FFMPEG_RUNTIME_DLLS "")
endif()

# 5. Optional Skia Ganesh/OpenGL Backend (MSVC ABI prebuilt such as JetBrains/skia-pack m144-e2e6623374-4)
option(STEM_ENABLE_SKIA "Link external Skia Ganesh OpenGL library when SKIA_ROOT is provided" OFF)
if(STEM_ENABLE_SKIA)
    if(NOT SKIA_ROOT AND DEFINED ENV{SKIA_ROOT} AND EXISTS "$ENV{SKIA_ROOT}")
        set(SKIA_ROOT "$ENV{SKIA_ROOT}" CACHE PATH "Prebuilt Skia root directory" FORCE)
    elseif(NOT SKIA_ROOT AND EXISTS "${CMAKE_SOURCE_DIR}/.stem_cache/toolchains/skia")
        set(SKIA_ROOT "${CMAKE_SOURCE_DIR}/.stem_cache/toolchains/skia" CACHE PATH "Prebuilt Skia root directory" FORCE)
    endif()
    if(NOT SKIA_ROOT)
        message(FATAL_ERROR "STEM_ENABLE_SKIA=ON requires -DSKIA_ROOT=<path-to-skia-pack>")
    endif()
    find_library(SKIA_LIBRARY
        NAMES skia
        HINTS
            "${SKIA_ROOT}/out/Release-windows-x64"
            "${SKIA_ROOT}/out/Release-x64"
            "${SKIA_ROOT}/lib"
        NO_DEFAULT_PATH
        REQUIRED
    )
    add_library(skia_ganesh INTERFACE)
    target_include_directories(skia_ganesh INTERFACE "${SKIA_ROOT}" "${SKIA_ROOT}/include")
    target_link_libraries(skia_ganesh INTERFACE "${SKIA_LIBRARY}")
    if(WIN32)
        target_link_libraries(skia_ganesh INTERFACE FontSub usp10 ole32 user32 gdi32 opengl32 advapi32 d3d12 d3dcompiler dxgi)
    endif()
    target_compile_definitions(skia_ganesh INTERFACE STEM_HAS_SKIA=1)
endif()
