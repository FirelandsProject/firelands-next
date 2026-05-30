add_executable(firelands-mmap-generator
    main.cpp
    MmapGenerator.cpp
)

target_include_directories(firelands-mmap-generator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(firelands-mmap-generator
    PRIVATE Recast
    PRIVATE Detour
)

set_target_properties(firelands-mmap-generator PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
