add_library(FirelandsVmap4Assembler STATIC
    TileAssembler.cpp
    WorldModelWrite.cpp
)
target_include_directories(FirelandsVmap4Assembler PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(FirelandsVmap4Assembler PUBLIC FirelandsVmapCommon)
target_compile_features(FirelandsVmap4Assembler PUBLIC cxx_std_17)

add_executable(firelands-vmap4-assembler assembler_main.cpp)
target_link_libraries(firelands-vmap4-assembler PRIVATE FirelandsVmap4Assembler)
