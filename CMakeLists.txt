cmake_minimum_required(VERSION 3.10)
project(Firelands CXX)

# --- Policies ---
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif()

set(CMAKE_POLICY_VERSION_MINIMUM 3.10)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# spdlog 1.14.x bundled fmt breaks under C++20 (FMT_STRING / consteval on Apple Clang).
# Use std::format instead of bundled fmt; requires C++20 (__cpp_lib_format).
add_compile_definitions(SPDLOG_USE_STD_FORMAT)

# --- Optimization Settings ---

# 1. ccache Support
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
    message(STATUS "ccache found and enabled: ${CCACHE_PROGRAM}")
endif()

# 2. Unity Build
option(ENABLE_UNITY_BUILD "Enable Unity Builds" OFF)
if(ENABLE_UNITY_BUILD)
    set(CMAKE_UNITY_BUILD ON)
endif()

# 3. Precompiled Headers Definition
set(PROJECT_PCH_HEADERS
    <vector>
    <string>
    <map>
    <unordered_map>
    <set>
    <unordered_set>
    <memory>
    <cstdint>
    <algorithm>
    <functional>
    <iostream>
    <chrono>
    <thread>
    <mutex>
    <nlohmann/json.hpp>
    <shared/Common.h>
    <shared/Logger.h>
)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

include(FetchContent)
add_subdirectory(deps)

# --- Dependencies ---

# OpenSSL & ZLIB (Crítico para BigInt y MariaDB)
find_package(OpenSSL REQUIRED)
if(WIN32)
    # Use the bundled zlib from deps/zlib — no system zlib on Windows.
    # CACHE + FORCE ensures MariaDB's internal find_package(ZLIB) respects
    # these values instead of overwriting them with ZLIB_LIBRARY-NOTFOUND.
    set(ZLIB_FOUND        TRUE                              CACHE BOOL   "" FORCE)
    set(ZLIB_INCLUDE_DIR  "${CMAKE_SOURCE_DIR}/deps/zlib"  CACHE PATH   "" FORCE)
    set(ZLIB_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/deps/zlib"  CACHE PATH   "" FORCE)
    set(ZLIB_LIBRARY      zlib                              CACHE STRING "" FORCE)
    set(ZLIB_LIBRARIES    zlib                              CACHE STRING "" FORCE)
    if(NOT TARGET ZLIB::ZLIB)
        add_library(ZLIB::ZLIB ALIAS zlib)
    endif()
else()
    find_package(ZLIB REQUIRED)
endif()

# Boost
find_package(Boost REQUIRED COMPONENTS thread)

# StormLib
set(STORM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(STORM_SKIP_INSTALL ON CACHE BOOL "" FORCE)
FetchContent_Declare(stormlib URL https://github.com/ladislav-zezula/StormLib/archive/refs/tags/v9.26.zip)
FetchContent_MakeAvailable(stormlib)

# GoogleTest
FetchContent_Declare(googletest URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# spdlog
FetchContent_Declare(spdlog URL https://github.com/gabime/spdlog/archive/refs/tags/v1.14.1.zip)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(spdlog)

# FTXUI
FetchContent_Declare(ftxui GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git GIT_TAG v5.0.0)
set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(ftxui)

# MariaDB Connector/C
set(WITH_SSL "OPENSSL" CACHE STRING "" FORCE)
set(WITH_EXTERNAL_ZLIB ON CACHE BOOL "" FORCE)
set(CONC_WITH_UNIT_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(mariadb-connector-c URL https://github.com/mariadb-corporation/mariadb-connector-c/archive/refs/tags/v3.3.8.zip)
FetchContent_Populate(mariadb-connector-c)

set(_plugin_file "${mariadb-connector-c_SOURCE_DIR}/cmake/install_plugins.cmake")
file(READ "${_plugin_file}" _content)
string(REPLACE "DESTINATION \${INSTALL_PLUGINDIR}" "RUNTIME DESTINATION lib/mariadb/plugin LIBRARY DESTINATION lib/mariadb/plugin" _content "${_content}")
file(WRITE "${_plugin_file}" "${_content}")

add_subdirectory(${mariadb-connector-c_SOURCE_DIR} ${mariadb-connector-c_BINARY_DIR})

# JSON
FetchContent_Declare(json URL https://github.com/nlohmann/json/archive/refs/tags/v3.11.2.zip)
FetchContent_MakeAvailable(json)

# yaml-cpp
FetchContent_Declare(yaml-cpp URL https://github.com/jbeder/yaml-cpp/archive/refs/tags/0.8.0.zip)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(yaml-cpp)

# Lua
FetchContent_Declare(lua_cmake URL https://github.com/walterschell/Lua/archive/refs/tags/v5.4.7.zip)
FetchContent_MakeAvailable(lua_cmake)

# MariaDB Connector/C++
FetchContent_Declare(mariadb-connector-cpp URL https://github.com/mariadb-corporation/mariadb-connector-cpp/archive/refs/tags/1.1.7.zip)
FetchContent_Populate(mariadb-connector-cpp)

set(MARIADB_CLIENT_LIBS mariadbclient CACHE STRING "" FORCE)
set(USE_SYSTEM_INSTALLED_LIB TRUE CACHE BOOL "" FORCE)
set(WITH_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS_ONLY OFF CACHE BOOL "" FORCE)
include_directories(SYSTEM ${mariadb-connector-c_SOURCE_DIR}/include ${mariadb-connector-c_BINARY_DIR}/include)
add_subdirectory(${mariadb-connector-cpp_SOURCE_DIR} ${mariadb-connector-cpp_BINARY_DIR})

if(APPLE)
    set(MARIADB_EXTRA_LIBS "-framework CoreFoundation -framework Security")
endif()

# --- Project Structure ---

include_directories(${PROJECT_SOURCE_DIR}/src)

add_subdirectory(src/shared)
add_subdirectory(src/domain)
add_subdirectory(src/application)
add_subdirectory(src/infrastructure)
add_subdirectory(tools/extractors)
add_subdirectory(tools/vmap)

# --- Executables ---

# AUTH SERVER
add_executable(auth 
    src/auth/main.cpp 
    src/auth/AuthApplication.cpp
    src/auth/AuthFtxuiConsole.cpp
)

# IMPORTANTE: Al saltar PCH, necesitamos asegurar que OpenSSL y otros headers 
# estén disponibles para estos archivos específicos.
set_source_files_properties(src/auth/AuthFtxuiConsole.cpp src/auth/AuthApplication.cpp
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

target_precompile_headers(auth PRIVATE ${PROJECT_PCH_HEADERS})

target_link_libraries(auth
    PRIVATE FirelandsInfrastructure
    PRIVATE FirelandsTui
    PRIVATE OpenSSL::SSL
    PRIVATE OpenSSL::Crypto
    PRIVATE Boost::thread
)

# WORLD SERVER
add_executable(world 
    src/world/main.cpp 
    src/world/WorldApplication.cpp
    src/world/WorldInteractiveConsole.cpp 
    src/world/WorldFtxuiConsole.cpp
)

set_source_files_properties(src/world/WorldFtxuiConsole.cpp src/world/WorldApplication.cpp
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

target_precompile_headers(world PRIVATE ${PROJECT_PCH_HEADERS})

target_link_libraries(world
    PRIVATE FirelandsInfrastructure
    PRIVATE FirelandsTui
    PRIVATE OpenSSL::SSL
    PRIVATE OpenSSL::Crypto
    PRIVATE Boost::thread
)

# DEVTOOLS CLI
add_executable(FirelandsDevTools
    src/tools/DevTools.cpp
)

target_precompile_headers(FirelandsDevTools PRIVATE ${PROJECT_PCH_HEADERS})

target_link_libraries(FirelandsDevTools
    PRIVATE FirelandsInfrastructure
    PRIVATE OpenSSL::SSL
    PRIVATE OpenSSL::Crypto
)

# --- Python Tools ---
find_program(FIRELANDS_PYTHON3 python3)
if(FIRELANDS_PYTHON3)
  add_custom_target(
    merge-migrations
    COMMAND "${FIRELANDS_PYTHON3}" "${CMAKE_SOURCE_DIR}/tools/merge_migrations.py"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Refresh sql/bundled/ via tools/merge_migrations.py"
    VERBATIM)
endif()

message(STATUS "Firelands WoW Emulator (arm64/macOS) project initialized.")

# --- Tests ---
add_subdirectory(tests)
