cmake_minimum_required(VERSION 3.13)

project(mem_alloc LANGUAGES C)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

add_executable(main
    ${CMAKE_CURRENT_SOURCE_DIR}/main.c
)


