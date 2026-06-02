cmake_minimum_required(VERSION 3.16)
project(minisql VERSION 0.1 LANGUAGES CXX)

# ── C++ standard ──────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Export compile commands for clangd ────────────────────────────────────────
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Compiler warnings ─────────────────────────────────────────────────────────
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
        -Wshadow
        -Wnon-virtual-dtor
        -Wcast-align
        -Woverloaded-virtual
    )
endif()

# ── Address / undefined-behaviour sanitisers (Debug builds) ──────────────────
option(ENABLE_SANITIZERS "Enable AddressSanitizer + UBSan in Debug" OFF)
if (ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options   (-fsanitize=address,undefined)
endif()

target_include_directories(minisql_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# ── Executable ────────────────────────────────────────────────────────────────
add_executable(minisql src/main.cpp)
target_link_libraries(minisql PRIVATE minisql_lib)

# ── Optional: copy compile_commands.json to project root for clangd ──────────
if (CMAKE_EXPORT_COMPILE_COMMANDS)
    add_custom_target(copy_compile_commands ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_BINARY_DIR}/compile_commands.json
            ${CMAKE_SOURCE_DIR}/compile_commands.json
        DEPENDS minisql_lib
    )
endif()
