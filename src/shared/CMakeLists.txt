add_library(FirelandsShared STATIC 
    Config.cpp
    system/SystemClipboard.cpp
    system/SystemBeep.cpp
    dbc/DbcReader.cpp
    dbc/CharStartOutfitDbc.cpp
    dbc/StarterSpellsDbc.cpp
    dbc/ItemDb2Wdb2.cpp
    dbc/ItemDbHotfixStore.cpp
    dbc/LanguagesDbc.cpp
    dbc/EmotesTextDbc.cpp
    dbc/SpellDifficultyDbc.cpp
    dbc/SpellVisualDbc.cpp
    dbc/GtOctCombatDbc.cpp
    dbc/GtPlayerStatGameTables.cpp
    dbc/FactionTemplateDbc.cpp
    dbc/ChrRacesDbc.cpp
    game/EquipmentCache.cpp
    game/ChatLanguages.cpp
    game/StarterSpellFilters.cpp
    game/StarterSkillFilters.cpp
    game/PlayerItemProficiency.cpp
    game/SkillLineCategories.cpp
    game/ItemEquipSlots.cpp
    game/PlayerGmAppearance.cpp
    game/StatFormulas.cpp
    game/ExperienceLogic.cpp
    game/RestExperienceLogic.cpp
    game/SpellPowerResolve.cpp
    game/ActionButton.cpp
    network/MovementWire.cpp
    network/movement/ClientMovementMse.cpp
    network/SpellCastWire.cpp
    network/PlaySpellVisualKitWire.cpp
    network/AuraUpdateWire.cpp
    network/SpellCooldownWire.cpp
    network/SpellPeriodicAuraLogWire.cpp
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

add_subdirectory(tui)
