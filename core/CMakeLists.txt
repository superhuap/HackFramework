file(
        GLOB_RECURSE src CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

add_library(Core STATIC ${src})

target_include_directories(Core PUBLIC include)

# Core sits on Utils (Logger, InputLock) and propagates it.
target_link_libraries(Core PUBLIC Utils)

target_compile_definitions(Core PRIVATE $<$<CONFIG:Debug>:DEBUG_BUILD>)

set_target_properties(Core PROPERTIES
        FOLDER "Libraries"
)
