cmake_minimum_required(VERSION 3.15)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

project(bfjit C)

add_executable(bfjit
    src/bfjit.c
    src/bfjit-codegen.c
    src/bfjit-compiler.c
    src/bfjit-debug-compiler.c
    src/bfjit-io.c
    src/bfjit-memory.c
    src/bfjit-runtime.c
    src/bfjit-time.c)

set_target_properties(bfjit PROPERTIES
    C_STANDARD 11
    INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/include
    INTERPROCEDURAL_OPTIMIZATION_RELEASE 1)

if(MSVC)
    target_compile_options(bfjit PRIVATE /W4)
else()
    target_compile_options(bfjit PRIVATE -Wall -Wextra)
endif()

option(BFJIT_TEST "Generate testing target" ON)

if(BFJIT_TEST)
    enable_testing()
    add_subdirectory(test)
endif()
