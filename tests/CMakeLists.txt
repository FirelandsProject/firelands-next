add_executable(FirelandsUnitTests
    unit/domain/UnitTests.cpp
    unit/infrastructure/MySqlAccountRepositoryTests.cpp
)

target_link_libraries(FirelandsUnitTests
    GTest::gtest_main
    FirelandsDomain
    FirelandsInfrastructure
)

include(GoogleTest)
gtest_discover_tests(FirelandsUnitTests)
