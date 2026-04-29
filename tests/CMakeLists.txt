add_executable(FirelandsUnitTests
    unit/domain/UnitTests.cpp
    unit/domain/RealmTests.cpp
    unit/infrastructure/MySqlAccountRepositoryTests.cpp
    unit/infrastructure/MySqlRealmRepositoryTests.cpp
    unit/shared/ByteBufferTests.cpp
    unit/shared/UpdateDataTests.cpp
    unit/shared/AuthPacketsTests.cpp
    unit/shared/LoggerTests.cpp
    unit/shared/ConfigTests.cpp
    unit/application/SRPServiceTests.cpp
    unit/application/RealmListServiceTests.cpp
    unit/application/AuthServiceTests.cpp
    unit/application/CharacterServiceTests.cpp
    unit/domain/MapTests.cpp
    unit/infrastructure/LuaGameScriptHostTests.cpp
)

target_link_libraries(FirelandsUnitTests
    GTest::gtest_main
    GTest::gmock
    FirelandsInfrastructure
)
target_precompile_headers(FirelandsUnitTests PRIVATE ${TEST_PCH_HEADERS})

include(GoogleTest)
gtest_discover_tests(FirelandsUnitTests)
