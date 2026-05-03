add_library(FirelandsDomain STATIC
    models/Realm.cpp
    world/Map.cpp
    world/Player.cpp
    world/Creature.cpp
    world/GameObject.cpp
)

target_include_directories(FirelandsDomain PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(FirelandsDomain PUBLIC 
    FirelandsShared
)
target_precompile_headers(FirelandsDomain PRIVATE ${PROJECT_PCH_HEADERS})
