add_library(FirelandsMapExtractorLib STATIC
    AdtReader.cpp
    WdtReader.cpp
    MapFileWriter.cpp
    MapExtractorTask.cpp
)

target_include_directories(FirelandsMapExtractorLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(FirelandsMapExtractorLib PUBLIC FirelandsVmapCommon)
target_compile_features(FirelandsMapExtractorLib PUBLIC cxx_std_20)

# MapTypes.h is header-only — no .cpp needed.

add_executable(firelands-map-extractor-vmap map_extractor_main.cpp)
target_link_libraries(firelands-map-extractor-vmap PRIVATE FirelandsMapExtractorLib)
