add_library(FirelandsShared STATIC 
    Config.cpp
    dbc/DbcReader.cpp
    dbc/CharStartOutfitDbc.cpp
    dbc/LanguagesDbc.cpp
    dbc/SpellDbc.cpp
    game/EquipmentCache.cpp
    game/ChatLanguages.cpp
    game/ItemEquipSlots.cpp
    network/MovementWire.cpp
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
