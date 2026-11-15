cmake_minimum_required(VERSION 3.20)
project(sdl3_triangle C)

# cmake -B build -DCMAKE_PREFIX_PATH=/path/to/MooGL-SDK
find_package(MooGL REQUIRED)
find_package(SDL3 REQUIRED CONFIG)

add_executable(sdl3_triangle main.c)
target_link_libraries(sdl3_triangle PRIVATE SDL3::SDL3 MooGL::GL)

set_target_properties(sdl3_triangle PROPERTIES BUILD_RPATH "@executable_path;${MooGL_LIBRARY_DIR}")
