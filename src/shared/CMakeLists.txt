add_library(FirelandsShared INTERFACE)

target_include_directories(FirelandsShared INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(FirelandsShared INTERFACE
    OpenSSL::SSL
    OpenSSL::Crypto
    spdlog::spdlog
)
