add_executable(FirelandsUnitTests
    unit/domain/UnitTests.cpp
    unit/domain/RealmTests.cpp
    unit/infrastructure/MySqlAccountRepositoryTests.cpp
    unit/infrastructure/MySqlRealmRepositoryTests.cpp
    unit/shared/ByteBufferTests.cpp
    unit/shared/AuthPacketsTests.cpp
    unit/shared/LoggerTests.cpp
    unit/application/SRPServiceTests.cpp
    unit/application/RealmListServiceTests.cpp
    unit/application/AuthServiceTests.cpp
    unit/application/CharacterServiceTests.cpp
)

target_link_libraries(FirelandsUnitTests
    GTest::gtest_main
    GTest::gmock
    FirelandsInfrastructure
)

include(GoogleTest)
gtest_discover_tests(FirelandsUnitTests)
