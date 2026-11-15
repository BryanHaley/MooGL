# Copyright (C) The MooGL Project
#
# find_package(glfw3) for MooGL's GLFW, which makes MooGL contexts. The
# target is glfw, as GLFW's own package names it.

get_filename_component(_moogl_glfw_root "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)

if(NOT TARGET glfw)
    add_library(glfw SHARED IMPORTED)
    set_target_properties(glfw PROPERTIES
        IMPORTED_LOCATION "${_moogl_glfw_root}/lib/libglfw.dylib"
        IMPORTED_SONAME "@rpath/libglfw.dylib"
        INTERFACE_INCLUDE_DIRECTORIES "${_moogl_glfw_root}/include")
endif()

set(glfw3_FOUND TRUE)
unset(_moogl_glfw_root)
