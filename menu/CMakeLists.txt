file(
        GLOB_RECURSE src CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

add_library(Menu STATIC ${src})

target_include_directories(Menu PUBLIC include)

target_link_libraries(Menu PUBLIC
        imgui::imgui
        Features
)

target_link_libraries(Menu PRIVATE
        Utils
)

set_target_properties(Menu PROPERTIES
        FOLDER "Libraries"
)
