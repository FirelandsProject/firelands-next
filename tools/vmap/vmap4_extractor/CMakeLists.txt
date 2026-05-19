add_executable(firelands-vmap4-extractor
    vmapexport.cpp
    Vmap4ExtractorCli.cpp
    adtfile.cpp
    wdtfile.cpp
    wmo.cpp
    model.cpp
    mpqfile.cpp
    gameobject_extract.cpp
    QuatMath.cpp
)

target_include_directories(firelands-vmap4-extractor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(firelands-vmap4-extractor PRIVATE FirelandsVmapCommon)
target_compile_features(firelands-vmap4-extractor PRIVATE cxx_std_20)
