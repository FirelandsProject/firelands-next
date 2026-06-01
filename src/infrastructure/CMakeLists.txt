add_library(FirelandsInfrastructure STATIC
    combat/InMemoryThreatManager.cpp
    combat/MySqlThreatManager.cpp
    combat/MySqlSpellProcessor.cpp
    dbc/SpellCastTablesDbc.cpp
    dbc/SpellEntryDbcStore.cpp
    persistence/MySqlAccountRepository.cpp
    persistence/MySqlRbacRepository.cpp
    persistence/MySqlAccountDataRepository.cpp
    persistence/MySqlRealmRepository.cpp
    persistence/MySqlCharacterRepository.cpp
    persistence/MySqlGmTicketRepository.cpp
    persistence/MySqlGossipRepository.cpp
    persistence/MySqlNpcTextRepository.cpp
    persistence/MySqlQuestGossipRepository.cpp
    persistence/MySqlPlayerCreateInfoRepository.cpp
    persistence/MySqlNpcTemplateSearchRepository.cpp
    persistence/MySqlCreatureSpawnRepository.cpp
    persistence/MySqlCreatureClassLevelStatsRepository.cpp
    persistence/MySqlPhaseGroupCatalogRepository.cpp
    persistence/MySqlPhaseAreaCatalogRepository.cpp
    persistence/MySqlPhaseConditionRepository.cpp
    persistence/MySqlPlayerQuestProgressRepository.cpp
    persistence/DatabaseMigrator.cpp
    world/DbCreatureSpawnBootstrap.cpp
    network/sessions/AuthSession.cpp
    network/sessions/WorldSession.cpp
    network/sessions/worldsession/WorldSessionClientHandlers.cpp
    network/sessions/worldsession/WorldSessionDispatcher.cpp
    network/sessions/worldsession/WorldSessionAccountDataHandlers.cpp
    network/sessions/worldsession/WorldSessionAuthHandlers.cpp
    network/sessions/worldsession/WorldSessionChatHandlers.cpp
    network/sessions/worldsession/WorldSessionEmoteHandlers.cpp
    network/sessions/worldsession/WorldSessionMovementHandlers.cpp
    network/sessions/worldsession/WorldSessionObjectUpdate.cpp
    network/sessions/worldsession/GmTicketPackets.cpp
    network/sessions/worldsession/WorldSessionGmState.cpp
    network/sessions/worldsession/WorldSessionGmTicketHandlers.cpp
    network/sessions/worldsession/WorldSessionGmTicketGossip.cpp
    network/sessions/worldsession/WorldSessionGmNpcInfoGossip.cpp
    network/sessions/worldsession/WorldSessionGmCommands.cpp
    network/sessions/worldsession/WorldSessionGossip.cpp
    network/sessions/worldsession/WorldSessionCharacterHandlers.cpp
    network/sessions/worldsession/WorldSessionItemHandlers.cpp
    network/sessions/worldsession/WorldSessionLoginFlow.cpp
    network/sessions/worldsession/WorldSessionPhasing.cpp
    network/sessions/worldsession/WorldSessionAreaPhasing.cpp
    network/sessions/worldsession/WorldSessionNetwork.cpp
    network/sessions/worldsession/WorldSessionOutboundPackets.cpp
    network/sessions/worldsession/WorldSessionSpellCast.cpp
    network/sessions/worldsession/WorldSessionSpellCooldown.cpp
    network/sessions/worldsession/WorldSessionSpellEffects.cpp
    network/sessions/worldsession/WorldSessionCombat.cpp
    network/sessions/worldsession/WorldSessionExperience.cpp
    world/MapAuraTicker.cpp
    world/MapPlayerRegenTicker.cpp
    network/sessions/worldsession/WorldSessionTimeMirror.cpp
    network/sessions/worldsession/WorldSessionWorldLifecycle.cpp
    network/sessions/worldsession/WorldSessionActionButtons.cpp
    network/asio/AsyncNetworkServer.cpp
    network/realm_link/RealmLiveRegistry.cpp
    network/realm_link/RealmLinkSession.cpp
    network/realm_link/RealmLinkOutbound.cpp
    network/rest/RestAuthServer.cpp
    scripting/LuaGameScriptHost.cpp
    world/MapCollisionQueriesStub.cpp
    collision/DetourNavMeshManager.cpp
    collision/MapCollisionQueriesReal.cpp
    collision/VMapManager2.cpp
    collision/WorldModelRuntime.cpp
)

set_source_files_properties(scripting/LuaGameScriptHost.cpp
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

target_link_libraries(FirelandsInfrastructure 
    PUBLIC FirelandsApplication
    PUBLIC mariadbcpp-static mariadbclient ${OPENSSL_LIBRARIES} ${MARIADB_EXTRA_LIBS}
    PUBLIC Boost::thread
    PUBLIC nlohmann_json::nlohmann_json
    PUBLIC Lua::Library
    PUBLIC Detour
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
