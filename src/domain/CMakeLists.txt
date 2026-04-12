add_library(FirelandsDomain INTERFACE)

target_include_directories(FirelandsDomain INTERFACE 
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(FirelandsDomain INTERFACE 
    FirelandsShared
)
