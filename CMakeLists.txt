cmake_minimum_required(VERSION 4.1)

set(CMAKE_SYSTEM_NAME Windows)
set(BUILD_ARCH "x64") # 目标架构：x86 = 32位，x64 = 64位

if(CMAKE_HOST_WIN32)
    set(VCPKG_TARGET_TRIPLET "${BUILD_ARCH}-windows-static")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    set(VCPKG_TARGET_TRIPLET "${BUILD_ARCH}-mingw-static")
    # Dead code elimination and a small DLL: the static libs (libstdc++,
    # vcpkg) carry DWARF debug sections that bloat the image otherwise.
    add_compile_options(-ffunction-sections -fdata-sections)
    add_link_options(-static -static-libgcc -static-libstdc++ -Wl,--gc-sections)
    add_link_options($<$<CONFIG:Release>:-s>)
endif()

project(HackFramework VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(spdlog CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(minhook CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Vulkan REQUIRED)

if(BUILD_ARCH STREQUAL "x64")
    # One backend per DLL: HackFramework_<BACKEND>.dll each.
    set(ENABLED_BACKENDS DX9 DX10 DX11 DX12 OPENGL VULKAN)
elseif(BUILD_ARCH STREQUAL "x86")
    set(ENABLED_BACKENDS DX9 DX10 DX11 OPENGL VULKAN)
endif()

add_subdirectory(utils)
add_subdirectory(core)
add_subdirectory(feature)
add_subdirectory(menu)
add_subdirectory(backend)