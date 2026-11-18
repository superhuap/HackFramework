file(
        GLOB_RECURSE src CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

add_library(Menu STATIC ${src})

target_include_directories(Menu PUBLIC include)

# System import libraries the vendored imgui backends need. Wrapped in an
# interface target declared after imgui so that they end up after the imgui
# static library in the link line (GNU ld resolves left to right).
add_library(WinSysLibs INTERFACE)
target_link_libraries(WinSysLibs INTERFACE d3dcompiler dwmapi dxgi)

target_link_libraries(Menu PUBLIC
        imgui::imgui
        WinSysLibs
        Features
)

target_link_libraries(Menu PRIVATE
        Utils
)

set_target_properties(Menu PROPERTIES
        FOLDER "Libraries"
)
