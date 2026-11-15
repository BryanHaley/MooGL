cmake_minimum_required(VERSION 3.20)
project(sfml_shapes CXX)

# cmake -B build -DCMAKE_PREFIX_PATH=/path/to/MooGL-SDK
find_package(MooGL REQUIRED)
find_package(SFML 3 REQUIRED COMPONENTS Graphics CONFIG)

add_executable(sfml_shapes main.cpp)
target_compile_features(sfml_shapes PRIVATE cxx_std_17)
target_link_libraries(sfml_shapes PRIVATE SFML::Graphics)

set_target_properties(sfml_shapes PROPERTIES BUILD_RPATH "@executable_path;${MooGL_LIBRARY_DIR}")
