cmake_minimum_required (VERSION 3.16)
project (HW)

add_library(nivisa SHARED IMPORTED)
set_target_properties(nivisa PROPERTIES 
    IMPORTED_IMPLIB "../lib/visa64.lib"
)

add_executable (hw1 pcontrol.cpp)

target_link_libraries(hw1 PUBLIC nivisa)
include_directories( ../include ../lib )