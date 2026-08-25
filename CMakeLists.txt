cmake_minimum_required(VERSION 3.20)

set(VCPKG_TARGET_TRIPLET "x64-windows-static")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(HackFramework VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_LOGGING "Enable framework logging" ON)
option(ENABLE_LOGGING_COLOR "Enable logging color" ON)

string(TIMESTAMP BUILD_TIMESTAMP "%Y-%m-%d %H:%M:%S")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in "${PROJECT_BINARY_DIR}/generated/config.h")

find_package(imgui CONFIG REQUIRED)
find_package(minhook CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Vulkan REQUIRED)

set(ENABLED_BACKENDS DX9 DX10 DX11 DX12 OPENGL VULKAN)
set(ENABLED_BACKENDS ${ENABLED_BACKENDS} CACHE INTERNAL "")

add_subdirectory(utils)
add_subdirectory(features)
add_subdirectory(menu)
add_subdirectory(backend)