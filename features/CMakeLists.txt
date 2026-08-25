file(
        GLOB_RECURSE src CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

add_library(Features STATIC ${src})

target_include_directories(Features PUBLIC include)

target_link_libraries(Features PRIVATE
        Menu
        Utils
        imgui::imgui
)

set_target_properties(Features PROPERTIES
        FOLDER "Libraries"
)