add_executable(FirelandsUnitTests
    unit/domain/UnitTests.cpp
    unit/domain/RealmTests.cpp
    unit/infrastructure/MySqlAccountRepositoryTests.cpp
    unit/infrastructure/MySqlRealmRepositoryTests.cpp
    unit/infrastructure/RealmLiveRegistryTests.cpp
    unit/shared/ByteBufferTests.cpp
    unit/shared/UpdateDataTests.cpp
    unit/shared/AuthPacketsTests.cpp
    unit/shared/LoggerTests.cpp
    unit/shared/ConfigTests.cpp
    unit/shared/SpellCastWireTests.cpp
    unit/application/SRPServiceTests.cpp
    unit/application/RealmListServiceTests.cpp
    unit/application/AuthServiceTests.cpp
    unit/application/CharacterServiceTests.cpp
    unit/application/WorldServiceCreatureScriptTests.cpp
    unit/domain/MapTests.cpp
    unit/infrastructure/LuaGameScriptHostTests.cpp
    unit/infrastructure/MapCollisionQueriesStubTests.cpp
    unit/tools/WowDataMpqListTests.cpp
)

target_link_libraries(FirelandsUnitTests
    GTest::gtest_main
    GTest::gmock
    FirelandsInfrastructure
    FirelandsExtractCommon
)
target_compile_definitions(FirelandsUnitTests PRIVATE
    FIRELANDS_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}")
target_precompile_headers(FirelandsUnitTests PRIVATE ${TEST_PCH_HEADERS})

include(GoogleTest)
gtest_discover_tests(FirelandsUnitTests)
