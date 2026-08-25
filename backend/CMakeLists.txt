set(DLLMAIN_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/../dllmain.cpp)

foreach(BACKEND ${ENABLED_BACKENDS})
    set(BACKEND_SOURCE src/${BACKEND}_Backend.cpp)

    if(BACKEND STREQUAL "DX9")
        set(BACKEND_LIBS d3d9 d3dx9)
        set(BACKEND_DEFINE BACKEND_DX9)
    elseif(BACKEND STREQUAL "DX10")
        set(BACKEND_LIBS d3d10 dxgi)
        set(BACKEND_DEFINE BACKEND_DX10)
    elseif(BACKEND STREQUAL "DX11")
        set(BACKEND_LIBS d3d11 dxgi)
        set(BACKEND_DEFINE BACKEND_DX11)
    elseif(BACKEND STREQUAL "DX12")
        set(BACKEND_LIBS d3d12 dxgi)
        set(BACKEND_DEFINE BACKEND_DX12)
    elseif(BACKEND STREQUAL "OPENGL")
        set(BACKEND_LIBS OpenGL::GL)
        set(BACKEND_DEFINE BACKEND_OPENGL)
    elseif(BACKEND STREQUAL "VULKAN")
        set(BACKEND_LIBS Vulkan::Vulkan)
        set(BACKEND_DEFINE BACKEND_VULKAN)
    endif()

    add_library(${BACKEND} SHARED
            ${DLLMAIN_SOURCE}
            ${BACKEND_SOURCE}
    )

    target_include_directories(${BACKEND} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/..
    )

    target_link_libraries(${BACKEND} PRIVATE
            Menu
            Features
            Utils
            imgui::imgui
            minhook::minhook
            ${BACKEND_LIBS}
    )

    target_compile_definitions(${BACKEND} PRIVATE
            ${BACKEND_DEFINE}
    )

    set_target_properties(${BACKEND} PROPERTIES
            OUTPUT_NAME "${PROJECT_NAME}_${BACKEND}"
            FOLDER "Backends"
            DEBUG_POSTFIX "_debug"
    )
endforeach()