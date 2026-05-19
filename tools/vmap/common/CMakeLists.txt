add_library(FirelandsVmapCommon STATIC
    VMapMagic.cpp
    MpqStream.cpp
    DbcReader.cpp
    ModelSpawn.cpp
    BoundingIntervalHierarchy.cpp
)

target_include_directories(FirelandsVmapCommon PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(FirelandsVmapCommon PUBLIC storm)

target_compile_features(FirelandsVmapCommon PUBLIC cxx_std_20)

# ChunkReader.h, Vec3.h, Mat3.h are header-only — no corresponding .cpp.
