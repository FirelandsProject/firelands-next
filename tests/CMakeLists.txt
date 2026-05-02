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
    unit/vmap/BihTests.cpp
    unit/vmap/ModelSpawnTests.cpp
    unit/vmap/Vec3Mat3Tests.cpp
    unit/vmap/MapFileWriterTests.cpp
    unit/vmap/QuatMathTests.cpp
    ${CMAKE_SOURCE_DIR}/tools/vmap/vmap4_extractor/QuatMath.cpp
)

target_link_libraries(FirelandsUnitTests
    GTest::gtest_main
    GTest::gmock
    FirelandsInfrastructure
    FirelandsExtractCommon
    FirelandsMapExtractorLib
)
target_compile_definitions(FirelandsUnitTests PRIVATE
    FIRELANDS_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}")
target_include_directories(FirelandsUnitTests PRIVATE
    ${CMAKE_SOURCE_DIR}/tools/vmap/vmap4_extractor)
target_precompile_headers(FirelandsUnitTests PRIVATE ${TEST_PCH_HEADERS})

include(GoogleTest)
gtest_discover_tests(FirelandsUnitTests)
