cmake_minimum_required(VERSION 3.30)
project(CHIP_8 C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake_modules)

set(SDL2_PATH "C:/sdl/SDL2-2.30.11/x86_64-w64-mingw32")

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIR})

add_executable(CHIP_8 main.c)

target_link_libraries(CHIP_8
        ${SDL2_LIBRARY}
        ${SDL2_PATH}/lib/libSDL2main.a  # SDL2main for the Windows entry point
        ${SDL2_PATH}/lib/libSDL2.dll.a  # Link SDL2 DLL (or .a for static)
)
