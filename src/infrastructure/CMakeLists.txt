add_library(FirelandsInfrastructure STATIC
    dbc/SpellCastTablesDbc.cpp
    dbc/SpellEntryDbcStore.cpp
    persistence/MySqlAccountRepository.cpp
    persistence/MySqlAccountDataRepository.cpp
    persistence/MySqlRealmRepository.cpp
    persistence/MySqlCharacterRepository.cpp
    persistence/MySqlGmTicketRepository.cpp
    persistence/MySqlPlayerCreateInfoRepository.cpp
    network/sessions/AuthSession.cpp
    network/sessions/WorldSession.cpp
    network/sessions/worldsession/WorldSessionDispatcher.cpp
    network/sessions/worldsession/WorldSessionAccountDataHandlers.cpp
    network/sessions/worldsession/WorldSessionAuthHandlers.cpp
    network/sessions/worldsession/WorldSessionChatHandlers.cpp
    network/sessions/worldsession/WorldSessionMovementHandlers.cpp
    network/sessions/worldsession/WorldSessionObjectUpdate.cpp
    network/sessions/worldsession/GmTicketPackets.cpp
    network/sessions/worldsession/WorldSessionGmState.cpp
    network/sessions/worldsession/WorldSessionGmTicketHandlers.cpp
    network/sessions/worldsession/WorldSessionCharacterHandlers.cpp
    network/sessions/worldsession/WorldSessionItemHandlers.cpp
    network/asio/AsyncNetworkServer.cpp
    network/realm_link/RealmLiveRegistry.cpp
    network/realm_link/RealmLinkSession.cpp
    network/realm_link/RealmLinkOutbound.cpp
    network/rest/RestAuthServer.cpp
    scripting/LuaGameScriptHost.cpp
    world/MapCollisionQueriesStub.cpp
)

set_source_files_properties(scripting/LuaGameScriptHost.cpp
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

target_link_libraries(FirelandsInfrastructure 
    PUBLIC FirelandsApplication
    PUBLIC mariadbcpp-static mariadbclient ${OPENSSL_LIBRARIES} ${MARIADB_EXTRA_LIBS}
    PUBLIC Boost::thread
    PUBLIC nlohmann_json::nlohmann_json
    PUBLIC Lua::Library
    PRIVATE ZLIB::ZLIB
)
target_precompile_headers(FirelandsInfrastructure PRIVATE ${PROJECT_PCH_HEADERS})

target_include_directories(FirelandsInfrastructure PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(FirelandsInfrastructure SYSTEM PUBLIC
    ${mariadb-connector-cpp_SOURCE_DIR}/include
    ${mariadb-connector-c_SOURCE_DIR}/include
    ${mariadb-connector-c_BINARY_DIR}/include
)
