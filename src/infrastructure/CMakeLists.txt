add_library(FirelandsInfrastructure STATIC
    persistence/MySqlAccountRepository.cpp
    persistence/MySqlRealmRepository.cpp
    persistence/MySqlCharacterRepository.cpp
    network/sessions/AuthSession.cpp
    network/sessions/WorldSession.cpp
    network/asio/AsyncNetworkServer.cpp
    network/rest/RestAuthServer.cpp
)

target_link_libraries(FirelandsInfrastructure 
    PUBLIC FirelandsApplication
    PUBLIC mariadbcpp-static mariadbclient ${OPENSSL_LIBRARIES} ${MARIADB_EXTRA_LIBS}
    PUBLIC Boost::thread
    PUBLIC nlohmann_json::nlohmann_json
)

target_include_directories(FirelandsInfrastructure PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(FirelandsInfrastructure SYSTEM PUBLIC
    ${mariadb-connector-cpp_SOURCE_DIR}/include
    ${mariadb-connector-c_SOURCE_DIR}/include
    ${mariadb-connector-c_BINARY_DIR}/include
)
