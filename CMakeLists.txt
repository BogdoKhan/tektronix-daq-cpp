cmake_minimum_required (VERSION 3.16)
project (HW)

add_library(nivisa SHARED IMPORTED)
if (WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set_target_properties(nivisa PROPERTIES 
        IMPORTED_IMPLIB "../lib/visa64.lib"
    )
else()
    set_target_properties(nivisa PROPERTIES 
        IMPORTED_IMPLIB "/usr/lib/x86_64-linux-gnu/libvisa.so"
        IMPORTED_LOCATION "/usr/lib/x86_64-linux-gnu/libvisa.so"
        INTERFACE_INCLUDE_DIRECTORIES  "/usr/include/ni-visa"
    )
    link_directories("/usr/lib/x86_64-linux-gnu/")
endif()


set(INCLUDE_PATH "${PROJECT_SOURCE_DIR}/include")
file(GLOB_RECURSE HPPS "${INCLUDE_PATH}/*.hpp" "${INCLUDE_PATH}/*.h")

find_package(GSL REQUIRED)

set(GSL_FIT_DIR ./include/gsl-curve-fit/)
#include_directories(${GSL_FIT_DIR})

set(GNUPL_DIR ./include/gnuplot-cpp/)
#include_directories(${GNUPL_DIR})

set(SOURCES "pcontrol.cpp" ${HPPS})
set(LIBRARIES "GSL::gsl;GSL::gslcblas")

add_executable (daq-sipm ${SOURCES} ${GSL_FIT_DIR}/curve_fit.cpp)
target_include_directories(daq-sipm PUBLIC ${INCLUDE_PATH} ${GSL_FIT_DIR} ${GNUPL_DIR})

target_link_libraries(daq-sipm PUBLIC nivisa ${LIBRARIES})
