cmake_minimum_required(VERSION 3.12)
project(dukemdx CXX)

file(GLOB_RECURSE DUKEMDX_HEADERS CONFIGURE_DEPENDS "include/*.h")
file(GLOB_RECURSE DUKEMDX_SOURCES CONFIGURE_DEPENDS "sources/*.cpp" "sources/*.def")
add_library(dukemdx SHARED ${DUKEMDX_HEADERS} ${DUKEMDX_SOURCES})
target_include_directories(dukemdx PRIVATE include)

if ("${CMAKE_SIZEOF_VOID_P}" STREQUAL "8")
    target_compile_definitions(dukemdx PRIVATE _NOE64)
endif()
