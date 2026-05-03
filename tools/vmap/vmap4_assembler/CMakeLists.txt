add_executable(firelands-vmap4-assembler
    assembler_main.cpp
    TileAssembler.cpp
    WorldModelWrite.cpp
)

target_include_directories(firelands-vmap4-assembler PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(firelands-vmap4-assembler PRIVATE FirelandsVmapCommon)
target_compile_features(firelands-vmap4-assembler PRIVATE cxx_std_17)
