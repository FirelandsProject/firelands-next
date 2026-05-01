add_library(FirelandsShared STATIC 
    Config.cpp
    dbc/DbcReader.cpp
    dbc/CharStartOutfitDbc.cpp
    game/EquipmentCache.cpp
    game/ItemEquipSlots.cpp
    network/SpellCastWire.cpp
)

target_include_directories(FirelandsShared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(FirelandsShared PUBLIC
    OpenSSL::SSL
    OpenSSL::Crypto
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    yaml-cpp
)
