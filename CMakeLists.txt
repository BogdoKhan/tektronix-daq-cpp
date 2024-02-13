cmake_minimum_required (VERSION 3.16)
project (HW)

add_library(nivisa SHARED IMPORTED)
set_target_properties(nivisa PROPERTIES 
    IMPORTED_IMPLIB "../lib/visa64.lib"
)

set(INCLUDE_PATH "${PROJECT_SOURCE_DIR}/include")
file(GLOB_RECURSE HPPS "${INCLUDE_PATH}/*.hpp" "${INCLUDE_PATH}/*.h")

set(SOURCES "pcontrol.cpp" ${HPPS})
add_executable (hw1 ${SOURCES})
target_include_directories(hw1 PUBLIC ${INCLUDE_PATH})

target_link_libraries(hw1 PUBLIC nivisa)
