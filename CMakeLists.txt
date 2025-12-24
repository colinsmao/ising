cmake_minimum_required(VERSION 3.22)
project(ising)

set(CMAKE_CXX_STANDARD 17)
if (APPLE)
    set(CMAKE_CXX_FLAGS "-framework Cocoa -framework IOKit")
    # -framework OpenGL -framework CoreVideo
    add_compile_definitions(GL_SILENCE_DEPRECATION)
endif ()
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -ffast-math")  # -fno-finite-math-only

include_directories(${PROJECT_SOURCE_DIR}/glad/include)
include_directories(${PROJECT_SOURCE_DIR}/glfw-3.3.7.bin.MACOS/include)
include_directories(${PROJECT_SOURCE_DIR}/includes)

# ImGui
include_directories(${PROJECT_SOURCE_DIR}/imgui)
include_directories(${PROJECT_SOURCE_DIR}/imgui/backends)
set(IMGUI_SOURCES
        ${PROJECT_SOURCE_DIR}/imgui/imgui.cpp
        ${PROJECT_SOURCE_DIR}/imgui/imgui_draw.cpp
        ${PROJECT_SOURCE_DIR}/imgui/imgui_tables.cpp
        ${PROJECT_SOURCE_DIR}/imgui/imgui_widgets.cpp
        ${PROJECT_SOURCE_DIR}/imgui/backends/imgui_impl_glfw.cpp
        ${PROJECT_SOURCE_DIR}/imgui/backends/imgui_impl_opengl3.cpp
)
add_compile_definitions(IMGUI_IMPL_OPENGL_LOADER_GLAD)

add_executable(ising
        program.cpp
        ${IMGUI_SOURCES}
)

link_directories(${PROJECT_SOURCE_DIR}/glfw-3.3.7.bin.MACOS/lib-arm64)
add_subdirectory(${PROJECT_SOURCE_DIR}/glad/)
target_link_libraries(ising ${PROJECT_SOURCE_DIR}/glfw-3.3.7.bin.MACOS/lib-arm64/libglfw3.a glad)
