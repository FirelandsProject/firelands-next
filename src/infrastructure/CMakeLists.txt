add_library(FirelandsInfrastructure STATIC
    persistence/MySqlAccountRepository.cpp
)

target_link_libraries(FirelandsInfrastructure 
    PRIVATE FirelandsDomain
    PUBLIC mariadbcpp-static mariadbclient ${OPENSSL_LIBRARIES} ${MARIADB_EXTRA_LIBS}
)

target_include_directories(FirelandsInfrastructure PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(FirelandsInfrastructure SYSTEM PUBLIC
    ${mariadb-connector-cpp_SOURCE_DIR}/include
    ${mariadb-connector-c_SOURCE_DIR}/include
    ${mariadb-connector-c_BINARY_DIR}/include
)
