cmake_minimum_required(VERSION 3.10)
project(Firelands CXX)

# Silence warnings from newer CMake versions
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW) # Use extraction time for FetchContent timestamps
endif()
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD) # Keep using FindBoost module for now
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
# spdlog must enter via <shared/Logger.h> so SPDLOG_LEVEL_NAMES (emoji levels) applies.
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

set(TEST_PCH_HEADERS
    ${PROJECT_PCH_HEADERS}
    <gtest/gtest.h>
    <gmock/gmock.h>
)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

include(FetchContent)

# 1. OpenSSL (Required by MariaDB)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(BZip2 REQUIRED)

# StormLib (MPQ) — pinned release; see STORM_LIB_ROADMAP.md
set(STORM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(STORM_SKIP_INSTALL ON CACHE BOOL "" FORCE)
FetchContent_Declare(
  stormlib
  URL https://github.com/ladislav-zezula/StormLib/archive/refs/tags/v9.26.zip
)
FetchContent_MakeAvailable(stormlib)

# 1.1 Boost (Required for Networking)
find_package(Boost REQUIRED COMPONENTS thread)
include_directories(${Boost_INCLUDE_DIRS})

# 2. GoogleTest
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# 2.1 spdlog (Logging)
FetchContent_Declare(
    spdlog
    URL https://github.com/gabime/spdlog/archive/refs/tags/v1.14.1.zip
)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS   OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL       OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(spdlog)

# 2.2 FTXUI (world server full-screen console UI)
FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG v5.0.0
)
set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FTXUI_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(ftxui)

# 3. MariaDB Connector/C
FetchContent_Declare(
    mariadb-connector-c
    URL https://github.com/mariadb-corporation/mariadb-connector-c/archive/refs/tags/v3.3.8.zip
    PATCH_COMMAND sed -i "" "s/VERSION 2.8.12/VERSION 3.5/g" CMakeLists.txt
)
set(WITH_SSL "OPENSSL" CACHE STRING "" FORCE)
set(WITH_EXTERNAL_ZLIB ON CACHE BOOL "" FORCE)
set(CONC_WITH_CURL OFF CACHE BOOL "" FORCE)
set(CONC_WITH_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(CONC_WITH_MYSQLCOMPAT ON CACHE BOOL "" FORCE)
set(INSTALL_LIBDIR "lib" CACHE STRING "" FORCE)
set(INSTALL_BINDIR "bin" CACHE STRING "" FORCE)
set(INSTALL_INCLUDEDIR "include" CACHE STRING "" FORCE)
set(INSTALL_PLUGINDIR "lib/mariadb/plugin" CACHE STRING "" FORCE)
FetchContent_MakeAvailable(mariadb-connector-c)

# 3.1 nlohmann_json
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/archive/refs/tags/v3.11.2.zip
    PATCH_COMMAND sed -i "" "s/VERSION 3.1/VERSION 3.10/g" CMakeLists.txt
)
FetchContent_MakeAvailable(json)

# 3.2 yaml-cpp
FetchContent_Declare(
    yaml-cpp
    URL https://github.com/jbeder/yaml-cpp/archive/refs/tags/0.8.0.zip
    PATCH_COMMAND sed -i "" "s/VERSION 3.4/VERSION 3.5/g" CMakeLists.txt
)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(yaml-cpp)

# 3.3 Lua 5.4 (gameplay scripting via C API; see docs/PARITY_AND_LUA_ROADMAP.md)
set(LUA_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(LUA_ENABLE_SHARED OFF CACHE BOOL "" FORCE)
set(LUA_BUILD_COMPILER OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  lua_cmake
  URL https://github.com/walterschell/Lua/archive/refs/tags/v5.4.7.zip
)
FetchContent_MakeAvailable(lua_cmake)

# 4. MariaDB Connector/C++ 1.1.7
FetchContent_Declare(
    mariadb-connector-cpp
    URL https://github.com/mariadb-corporation/mariadb-connector-cpp/archive/refs/tags/1.1.7.zip
    PATCH_COMMAND sed -i "" "s/VERSION 2.8.5/VERSION 3.5/g" CMakeLists.txt
)
FetchContent_Populate(mariadb-connector-cpp)

set(MARIADB_CLIENT_LIBS mariadbclient CACHE STRING "" FORCE)
set(USE_SYSTEM_INSTALLED_LIB TRUE CACHE BOOL "" FORCE)
set(WITH_UNIT_TESTS OFF CACHE BOOL "" FORCE)
include_directories(SYSTEM ${mariadb-connector-c_SOURCE_DIR}/include ${mariadb-connector-c_BINARY_DIR}/include)
add_subdirectory(${mariadb-connector-cpp_SOURCE_DIR} ${mariadb-connector-cpp_BINARY_DIR})

if(APPLE)
    set(MARIADB_EXTRA_LIBS "-framework CoreFoundation -framework Security")
endif()

# Global include for the project source
include_directories(${PROJECT_SOURCE_DIR}/src)

# Main project subdirectories
add_subdirectory(src/shared)
add_subdirectory(src/domain)
add_subdirectory(src/application)
add_subdirectory(src/infrastructure)
add_subdirectory(tools/extractors)
add_subdirectory(tools/vmap)
add_subdirectory(tests)

# Main Executables
add_executable(auth src/auth/main.cpp src/auth/AuthFtxuiConsole.cpp)
set_source_files_properties(src/auth/AuthFtxuiConsole.cpp
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
target_precompile_headers(auth PRIVATE ${PROJECT_PCH_HEADERS})
target_link_libraries(auth
    PRIVATE FirelandsInfrastructure
    PRIVATE ftxui::component
)

add_executable(world src/world/main.cpp src/world/WorldInteractiveConsole.cpp
    src/world/WorldFtxuiConsole.cpp)
set_source_files_properties(src/world/WorldFtxuiConsole.cpp
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
target_precompile_headers(world PRIVATE ${PROJECT_PCH_HEADERS})
target_link_libraries(world
    PRIVATE FirelandsInfrastructure
    PRIVATE ftxui::component
)

# Merge sql/*.sql into sql/merged/<database>.sql (one file per detected DB)
find_program(MERGE_MIGRATIONS_PYTHON python3)
if(MERGE_MIGRATIONS_PYTHON)
  add_custom_target(merge-migrations
    COMMAND ${MERGE_MIGRATIONS_PYTHON} ${CMAKE_SOURCE_DIR}/tools/merge_migrations.py
            --input-dir ${CMAKE_SOURCE_DIR}/sql
            --output-dir ${CMAKE_SOURCE_DIR}/sql/merged
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Condensing SQL migrations into sql/merged/ (per database)"
    VERBATIM
  )
else()
  message(STATUS "python3 not found; target merge-migrations will not be defined")
endif()

# Tools
add_executable(FirelandsDevTools src/tools/DevTools.cpp)
target_precompile_headers(FirelandsDevTools PRIVATE ${PROJECT_PCH_HEADERS})
target_link_libraries(FirelandsDevTools
    PRIVATE FirelandsInfrastructure
)

message(STATUS "Firelands WoW Emulator (4.3.4.15595) project initialized.")
