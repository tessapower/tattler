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
# Dependencies — vcpkg
# --------------------------------------------------------------------------

# Detours (no cmake config file — find manually, vcpkg toolchain sets search paths)
find_path(DETOURS_INCLUDE_DIR "detours/detours.h" REQUIRED)
find_library(DETOURS_LIBRARY detours REQUIRED)

add_library(detours::detours STATIC IMPORTED)
set_target_properties(detours::detours PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${DETOURS_INCLUDE_DIR}"
    IMPORTED_LOCATION "${DETOURS_LIBRARY}"
)

if(TATTLER_BUILD_VIEWER)
    find_package(imgui CONFIG REQUIRED)
endif()

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
        detours::detours
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
    file(GLOB_RECURSE VIEWER_SOURCES src/viewer/*.cpp)
    file(GLOB_RECURSE VIEWER_HEADERS include/viewer/*.h)
    add_executable(tattler WIN32 ${VIEWER_SOURCES} ${VIEWER_HEADERS})
    target_include_directories(tattler PRIVATE include)
    target_precompile_headers(tattler REUSE_FROM tattler_common)
    target_link_libraries(tattler PRIVATE
        tattler_common
        detours::detours
        imgui::imgui
        d3d12.lib
        dxgi
        dxguid
        d3dcompiler
    )

    # Copy resources folder to build directory
    add_custom_command(TARGET tattler POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/resources $<TARGET_FILE_DIR:tattler>/resources
        COMMENT "Copying resources to build directory"
    )
endif()

# --------------------------------------------------------------------------
# Google Test
# --------------------------------------------------------------------------
option(TATTLER_BUILD_TESTS "Build tests" ON)
if(TATTLER_BUILD_TESTS)
    enable_testing()

    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/releases/download/v1.17.0/googletest-1.17.0.tar.gz
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    file(GLOB_RECURSE TEST_SOURCES tests/*.cpp)
    add_executable(tattler_tests ${TEST_SOURCES})
    target_include_directories(tattler_tests PRIVATE include)
    target_link_libraries(tattler_tests PRIVATE
        tattler_common
        GTest::gtest
        GTest::gtest_main
    )
    target_precompile_headers(tattler_tests REUSE_FROM tattler_common)

    include(GoogleTest)
    gtest_discover_tests(tattler_tests)
endif()
