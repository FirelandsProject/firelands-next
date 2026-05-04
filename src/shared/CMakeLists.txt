add_library(FirelandsShared STATIC 
    Config.cpp
    system/SystemClipboard.cpp
    dbc/DbcReader.cpp
    dbc/CharStartOutfitDbc.cpp
    dbc/ItemDb2Wdb2.cpp
    dbc/ItemDbHotfixStore.cpp
    dbc/LanguagesDbc.cpp
    dbc/SpellDifficultyDbc.cpp
    dbc/GtOctCombatDbc.cpp
    dbc/GtPlayerStatGameTables.cpp
    dbc/FactionTemplateDbc.cpp
    dbc/ChrRacesDbc.cpp
    game/EquipmentCache.cpp
    game/ChatLanguages.cpp
    game/ItemEquipSlots.cpp
    game/PlayerGmAppearance.cpp
    game/StatFormulas.cpp
    network/MovementWire.cpp
    network/movement/ClientMovementMse.cpp
    network/SpellCastWire.cpp
)

target_include_directories(FirelandsShared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(FirelandsShared PUBLIC
    OpenSSL::SSL
    OpenSSL::Crypto
    spdlog::spdlog_header_only
    nlohmann_json::nlohmann_json
    yaml-cpp
)

if(WIN32)
  # Propagate to executables that link FirelandsShared (clipboard Win32 APIs).
  target_link_libraries(FirelandsShared PUBLIC user32)
endif()
