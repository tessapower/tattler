cmake_minimum_required(VERSION 3.28)
project(Tattler VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --------------------------------------------------------------------------
# Options
# --------------------------------------------------------------------------
option(TATTLER_BUILD_VIEWER "Build the Tattler viewer application" ON)
option(TATTLER_BUILD_HOOK   "Build the Tattler hook DLL"           ON)

# --------------------------------------------------------------------------
# Dependencies — FetchContent
# --------------------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(
    detours
    GIT_REPOSITORY https://github.com/microsoft/Detours.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(detours)

if(TATTLER_BUILD_VIEWER)
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        docking
    )
    FetchContent_MakeAvailable(imgui)
endif()

# --------------------------------------------------------------------------
# Detours — build as a static library from fetched sources
# --------------------------------------------------------------------------
file(GLOB DETOURS_SOURCES ${detours_SOURCE_DIR}/src/*.cpp)
list(FILTER DETOURS_SOURCES EXCLUDE REGEX "uimports\\.cpp$")
add_library(detours_lib STATIC ${DETOURS_SOURCES})
target_include_directories(detours_lib PUBLIC ${detours_SOURCE_DIR}/src)
target_compile_definitions(detours_lib PRIVATE WIN32_LEAN_AND_MEAN)

# --------------------------------------------------------------------------
# Shared library — common types used by both hook and viewer
# --------------------------------------------------------------------------
file(GLOB_RECURSE COMMON_SOURCES src/common/*.cpp)
file(GLOB_RECURSE COMMON_HEADERS include/common/*.h)
add_library(tattler_common STATIC ${COMMON_SOURCES} ${COMMON_HEADERS})
target_include_directories(tattler_common PUBLIC include src)
target_precompile_headers(tattler_common PUBLIC include/stdafx.h)

# --------------------------------------------------------------------------
# Hook DLL — injected into the target application via Detours
# --------------------------------------------------------------------------
if(TATTLER_BUILD_HOOK)
    file(GLOB_RECURSE HOOK_SOURCES src/hook/*.cpp)
    file(GLOB_RECURSE HOOK_HEADERS include/hook/*.h)

    add_library(tattler_hook SHARED ${HOOK_SOURCES} ${HOOK_HEADERS})
    target_include_directories(tattler_hook PRIVATE include)
    target_link_libraries(tattler_hook PRIVATE
        tattler_common
        detours_lib
        d3d12.lib
        dxgi
        dxguid
    )
    target_precompile_headers(tattler_hook REUSE_FROM tattler_common)
    set_target_properties(tattler_hook PROPERTIES
        OUTPUT_NAME "tattler_hook"
        PREFIX      ""
    )
endif()

# --------------------------------------------------------------------------
# Viewer — standalone Dear ImGui application
# --------------------------------------------------------------------------
if(TATTLER_BUILD_VIEWER)
    # Build Dear ImGui as a static lib with D3D12 + Win32 backends
    file(GLOB IMGUI_CORE_SOURCES ${imgui_SOURCE_DIR}/*.cpp)
    add_library(imgui_lib STATIC
        ${IMGUI_CORE_SOURCES}
        ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
    )
    target_include_directories(imgui_lib PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )
    target_link_libraries(imgui_lib PUBLIC
        d3d12.lib
        dxgi
        dxguid
        d3dcompiler
    )

    file(GLOB_RECURSE VIEWER_SOURCES src/viewer/*.cpp)
    file(GLOB_RECURSE VIEWER_HEADERS include/viewer/*.h)
    add_executable(tattler WIN32 ${VIEWER_SOURCES} ${VIEWER_HEADERS})
    target_include_directories(tattler PRIVATE include)
    target_precompile_headers(tattler REUSE_FROM tattler_common)
    target_link_libraries(tattler PRIVATE
        tattler_common
        detours_lib
        imgui_lib
    )
endif()
