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
include_directories(/opt/homebrew/Cellar/freetype/2.13.3/include/freetype2)

# find_package(OpenMP REQUIRED)
add_executable(ising program.cpp)
# add_subdirectory(${PROJECT_SOURCE_DIR}/render)
# target_link_libraries(boids OpenMP::OpenMP_CXX)

link_directories(${PROJECT_SOURCE_DIR}/glfw-3.3.7.bin.MACOS/lib-arm64)
add_subdirectory(${PROJECT_SOURCE_DIR}/glad/)
target_link_libraries(ising ${PROJECT_SOURCE_DIR}/glfw-3.3.7.bin.MACOS/lib-arm64/libglfw3.a glad)
target_link_libraries(ising /opt/homebrew/Cellar/freetype/2.13.3/lib/libfreetype.6.dylib)
