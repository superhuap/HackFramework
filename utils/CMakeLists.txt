file(
        GLOB_RECURSE src CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

add_library(Utils STATIC ${src})

target_include_directories(Utils PUBLIC include)
target_include_directories(Utils PRIVATE "${PROJECT_BINARY_DIR}/generated")

target_link_libraries(Utils PRIVATE spdlog::spdlog)

set_target_properties(Utils PROPERTIES
        FOLDER "Libraries"
)