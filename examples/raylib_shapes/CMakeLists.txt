cmake_minimum_required(VERSION 3.20)
project(raylib_shapes C)

# cmake -B build -DCMAKE_PREFIX_PATH=/path/to/MooGL-SDK
find_package(MooGL REQUIRED)
find_package(raylib REQUIRED CONFIG)

add_executable(raylib_shapes main.c)
target_link_libraries(raylib_shapes PRIVATE raylib)

set_target_properties(raylib_shapes PROPERTIES BUILD_RPATH "@executable_path;${MooGL_LIBRARY_DIR}")
