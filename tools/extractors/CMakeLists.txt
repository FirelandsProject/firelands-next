add_library(FirelandsExtractCommon STATIC
    WowDataMpqList.cpp
    MpqPatchChain.cpp
    ExtractorTasks.cpp
)

target_include_directories(FirelandsExtractCommon PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(FirelandsExtractCommon PUBLIC storm)
target_compile_features(FirelandsExtractCommon PUBLIC cxx_std_20)
if(WIN32)
    target_compile_definitions(FirelandsExtractCommon PUBLIC UNICODE _UNICODE)
endif()

add_executable(firelands-dbc-extractor dbc_extractor_main.cpp)
target_link_libraries(firelands-dbc-extractor PRIVATE FirelandsExtractCommon)

add_executable(firelands-map-extractor map_extractor_main.cpp)
target_link_libraries(firelands-map-extractor PRIVATE FirelandsExtractCommon)

add_executable(firelands-extractors
    extractors_shell_main.cpp
    ExtractorsFtxui.cpp
    ExtractorsCollisionTasks.cpp
)
set_source_files_properties(ExtractorsFtxui.cpp
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
target_link_libraries(firelands-extractors PRIVATE
    FirelandsExtractCommon
    FirelandsShared
    ftxui::component
)
set(_firelands_collision_bin "${CMAKE_BINARY_DIR}/bin")
target_compile_definitions(firelands-extractors PRIVATE
    "FIRELANDS_MAP_EXTRACTOR_PATH=\"${_firelands_collision_bin}/firelands-map-extractor-vmap${CMAKE_EXECUTABLE_SUFFIX}\""
    "FIRELANDS_VMAP4_EXTRACTOR_PATH=\"${_firelands_collision_bin}/firelands-vmap4-extractor${CMAKE_EXECUTABLE_SUFFIX}\""
    "FIRELANDS_VMAP4_ASSEMBLER_PATH=\"${_firelands_collision_bin}/firelands-vmap4-assembler${CMAKE_EXECUTABLE_SUFFIX}\""
)
add_dependencies(firelands-extractors
    firelands-map-extractor
    firelands-map-extractor-vmap
    firelands-vmap4-extractor
    firelands-vmap4-assembler
)
