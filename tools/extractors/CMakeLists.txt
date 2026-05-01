add_library(FirelandsExtractCommon STATIC
    WowDataMpqList.cpp
    MpqPatchChain.cpp
    ExtractorTasks.cpp
    ExtractorInteractive.cpp
)

target_include_directories(FirelandsExtractCommon PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(FirelandsExtractCommon PUBLIC storm)
target_compile_features(FirelandsExtractCommon PUBLIC cxx_std_17)

add_executable(firelands-dbc-extractor dbc_extractor_main.cpp)
target_link_libraries(firelands-dbc-extractor PRIVATE FirelandsExtractCommon)

add_executable(firelands-map-extractor map_extractor_main.cpp)
target_link_libraries(firelands-map-extractor PRIVATE FirelandsExtractCommon)

add_executable(firelands-extractors extractors_shell_main.cpp)
target_link_libraries(firelands-extractors PRIVATE FirelandsExtractCommon)
