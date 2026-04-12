cmake_minimum_required(VERSION 3.10)
project(Firelands CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

include(FetchContent)

# 1. OpenSSL (Required by MariaDB)
find_package(OpenSSL REQUIRED)

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
add_subdirectory(tests)

# Main Executables
add_executable(auth src/auth/main.cpp)
target_link_libraries(auth 
    PRIVATE FirelandsInfrastructure
)

add_executable(world src/world/main.cpp)
target_link_libraries(world 
    PRIVATE FirelandsInfrastructure
)

# Tools
add_executable(FirelandsDevTools src/tools/DevTools.cpp)
target_link_libraries(FirelandsDevTools
    PRIVATE FirelandsInfrastructure
)

message(STATUS "Firelands WoW Emulator (4.3.4.15595) project initialized.")
