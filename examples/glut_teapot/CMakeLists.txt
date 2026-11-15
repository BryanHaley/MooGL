cmake_minimum_required(VERSION 3.20)
project(glut_teapot C)

# cmake -B build -DCMAKE_PREFIX_PATH=/path/to/MooGL-SDK
find_package(MooGL REQUIRED)

add_executable(glut_teapot main.c)
target_link_libraries(glut_teapot PRIVATE MooGL::glut)

set_target_properties(glut_teapot PROPERTIES BUILD_RPATH "@executable_path;${MooGL_LIBRARY_DIR}")
