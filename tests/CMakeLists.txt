add_executable(FirelandsUnitTests
    unit/domain/UnitTests.cpp
    unit/domain/CharacterDisplayIdTests.cpp
    unit/domain/RealmTests.cpp
    unit/domain/AuraTests.cpp
    unit/domain/SpellDefinitionTests.cpp
    unit/domain/GossipMenuTests.cpp
    unit/domain/NpcTextTests.cpp
    unit/infrastructure/MySqlAccountRepositoryTests.cpp
    unit/infrastructure/MySqlRealmRepositoryTests.cpp
    unit/infrastructure/MySqlPlayerQuestProgressRepositoryTests.cpp
    unit/infrastructure/RealmLiveRegistryTests.cpp
    unit/infrastructure/SqlStatementSplitterTests.cpp
    unit/infrastructure/DatabaseMigrationCatalogTests.cpp
    unit/shared/ByteBufferTests.cpp
    unit/shared/UpdateDataTests.cpp
    unit/shared/AuthPacketsTests.cpp
    unit/shared/LoggerTests.cpp
    unit/shared/FtxuiLoggerBindingTests.cpp
    unit/shared/FtxuiTextClipboardTests.cpp
    unit/shared/ConfigTests.cpp
    unit/shared/ItemDb2Wdb2Tests.cpp
    unit/shared/FactionTemplateDbcTests.cpp
    unit/shared/EmotesTextDbcTests.cpp
    unit/shared/StarterSpellFiltersTests.cpp
    unit/shared/StarterSkillFiltersTests.cpp
    unit/shared/ChatLanguagesTests.cpp
    unit/shared/PlayerClassTests.cpp
    unit/shared/PlayerItemProficiencyTests.cpp
    unit/shared/SkillLineCategoriesTests.cpp
    unit/shared/FactionTemplateHelpersTests.cpp
    unit/shared/PlayerRaceFactionTemplateTests.cpp
    unit/shared/ChrRacesDbcTests.cpp
    unit/shared/NameGenDbcTests.cpp
    unit/shared/PermissionsTests.cpp
    unit/shared/PermissionNamesTests.cpp
    unit/shared/SpellCastWireTests.cpp
    unit/shared/SpellVisualDbcTests.cpp
    unit/shared/PlaySpellVisualKitWireTests.cpp
    unit/shared/AuraUpdateWireTests.cpp
    unit/shared/SpellCooldownWireTests.cpp
    unit/shared/ActionButtonTests.cpp
    unit/shared/ActionBarTogglesTests.cpp
    unit/shared/PackedPlayerGuidWireTests.cpp
    unit/shared/KnownSpellsWireTests.cpp
    unit/shared/SpellPeriodicAuraLogWireTests.cpp
    unit/shared/SpellEffectMagnitudeTests.cpp
    unit/shared/ExperienceLogicTests.cpp
    unit/shared/SpellPowerResolveTests.cpp
    unit/shared/SpellPowerCostTests.cpp
    unit/shared/MovementStateQueriesTests.cpp
    unit/shared/CombatPacketsTest.cpp
    unit/shared/ExperiencePacketsTest.cpp
    unit/shared/RestExperienceLogicTests.cpp
    unit/shared/CombatDamageTests.cpp
    unit/shared/PhaseShiftTests.cpp
    unit/shared/GmCreatureVisibilityTests.cpp
    unit/shared/PlayerResourceRegenTests.cpp
    unit/shared/SpellCastHasteTests.cpp
    unit/application/PlayerAuraRegenEffectsTests.cpp
    unit/shared/MeleeRangeTest.cpp
    unit/shared/WorldSessionMovementChecksTest.cpp
    unit/shared/MonsterMovePacketsTest.cpp
    unit/application/SRPServiceTests.cpp
    unit/application/RealmListServiceTests.cpp
    unit/application/AuthServiceTests.cpp
    unit/application/CharacterServiceTests.cpp
    unit/application/CharacterServiceRandomNameTests.cpp
    unit/application/CharacterActionButtonsTests.cpp
    unit/application/SpellHitEffectsTests.cpp
    unit/application/SpellImpactEffectsTests.cpp
    unit/application/SpellManagerTests.cpp
    unit/application/PlayerCreateInfoServiceStatsTests.cpp
    unit/application/PlayerSpellbookTests.cpp
    unit/application/PassiveSpellAurasTests.cpp
    unit/application/PlayerPhaseShiftTests.cpp
    unit/application/PhaseAreaCatalogTests.cpp
    unit/application/PhaseConditionEvaluatorTests.cpp
    unit/application/CommandServiceReviveUnlearnTests.cpp
    unit/application/CommandServiceRbacTests.cpp
    unit/application/GmPlayerReviveTests.cpp
    unit/application/PlayerAuraStatEffectsTests.cpp
    unit/application/MapServiceTests.cpp
    unit/application/MapRegistryTests.cpp
    unit/application/WorldServiceMapRegistryTests.cpp
    unit/application/WorldServiceCreatureScriptTests.cpp
    unit/application/CreatureSpawnLogicTests.cpp
    unit/application/CreatureProximityAggroTests.cpp
    unit/application/GossipLogicTests.cpp
    unit/application/QuestGiverLogicTests.cpp
    unit/application/QuestProgressLogicTests.cpp
    unit/application/QuestGossipFlagsTests.cpp
    unit/application/PlayerQuestProgressStoreTests.cpp
    unit/application/QuestGossipFilterTests.cpp
    unit/shared/WowGuidTests.cpp
    unit/shared/AreaTableDbcTests.cpp
    unit/infrastructure/ActionButtonPacketTests.cpp
    unit/infrastructure/ActionBarTogglesObjectUpdateTests.cpp
    unit/infrastructure/Power1ObjectUpdateTests.cpp
    unit/infrastructure/UnitObjectUpdateTests.cpp
    unit/infrastructure/PlayerBaselineStatsObjectUpdateTests.cpp
    unit/infrastructure/GossipPacketTests.cpp
    unit/infrastructure/VendorPacketTests.cpp
    unit/infrastructure/GmTicketGossipUiTests.cpp
    unit/domain/MapSnapshotTests.cpp
    unit/domain/MapTests.cpp
    unit/domain/CreatureCombatTests.cpp
    unit/domain/CreatureEvadeTests.cpp
    unit/domain/CreatureCombatStateTests.cpp
    unit/domain/CreatureUnitFlagsTests.cpp
    unit/domain/PlayerCombatTests.cpp
    unit/combat/CombatEngineTest.cpp
    unit/combat/CombatHostilityTests.cpp
    unit/combat/CreatureChaseMovementTest.cpp
    integration/combat/CombatIntegrationTest.cpp
    unit/infrastructure/LuaGameScriptHostTests.cpp
    unit/infrastructure/MapAuraTickerTests.cpp
    unit/infrastructure/MapCollisionQueriesStubTests.cpp
    unit/infrastructure/VMapManager2Tests.cpp
    unit/infrastructure/SpellCastTablesSpellPowerTests.cpp
    unit/infrastructure/SpellCastTablesDurationTests.cpp
    # TODO: fix missing headers for vmap/tools tests
    # unit/tools/WowDataMpqListTests.cpp
    # unit/vmap/BihTests.cpp
    # unit/vmap/ModelSpawnTests.cpp
    # unit/vmap/Vec3Mat3Tests.cpp
    # unit/vmap/MapFileWriterTests.cpp
    # unit/vmap/QuatMathTests.cpp
    # unit/vmap/TileAssemblerIntegrationTests.cpp
    # unit/vmap/Vmap4ExtractorCliTests.cpp
    # unit/vmap/MapExtractorParityTests.cpp
    # ${CMAKE_SOURCE_DIR}/tools/vmap/vmap4_extractor/QuatMath.cpp
    # ${CMAKE_SOURCE_DIR}/tools/vmap/vmap4_extractor/Vmap4ExtractorCli.cpp
)

target_link_libraries(FirelandsUnitTests
    GTest::gtest_main
    GTest::gmock
    FirelandsTui
    FirelandsInfrastructure
    FirelandsExtractCommon
    FirelandsMapExtractorLib
    FirelandsVmap4Assembler
)
target_compile_definitions(FirelandsUnitTests PRIVATE
    FIRELANDS_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}")
target_include_directories(FirelandsUnitTests PRIVATE
    ${CMAKE_SOURCE_DIR}/tools/vmap/vmap4_extractor)
target_precompile_headers(FirelandsUnitTests PRIVATE ${TEST_PCH_HEADERS})

include(GoogleTest)
gtest_discover_tests(FirelandsUnitTests)
