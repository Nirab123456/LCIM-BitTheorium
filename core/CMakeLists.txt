cmake_minimum_required(VERSION 3.10)
project(AtomicCIM LANGUAGES CXX)

# Use C++20 (change if you prefer C++17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Source layout - adjust these if your folders differ
set(HEADERS_DIR ${CMAKE_SOURCE_DIR}/headers)
set(SRC_DIR     ${CMAKE_SOURCE_DIR}/programs)

# Include headers
include_directories(${HEADERS_DIR})

# List source files
set(SRC_FILES
    ${SRC_DIR}/AtomicCSCompact.cpp
    ${CMAKE_SOURCE_DIR}/main.cpp
)

add_executable(AtomicCIM ${SRC_FILES})

# Compiler warnings - adjust per your compiler
if (MSVC)
    target_compile_options(AtomicCIM PRIVATE /W4 /WX)
else()
    target_compile_options(AtomicCIM PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
