cmake_minimum_required(VERSION 2.5)
project(VoorraadTellen C)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -pedantic")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O3")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -ggdb -Og")

include_directories(
    lib/
    src/
)

file(GLOB_RECURSE SOURCES src/*)
file(GLOB_RECURSE HEADERS src/*.h)
file(GLOB_RECURSE LIBRARY_SOURCES lib/*)
add_executable(VoorraadTellen ${SOURCES} ${LIBRARY_SOURCES})
