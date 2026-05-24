add_library(FirelandsTui STATIC
    FtxuiAnsi.cpp
    FtxuiBanner.cpp
    FtxuiLogSink.cpp
    FtxuiLogSpdlog.cpp
    FtxuiLogView.cpp
    FtxuiTextClipboard.cpp
)

target_include_directories(FirelandsTui PUBLIC ${CMAKE_SOURCE_DIR}/src)

set_source_files_properties(
    FtxuiBanner.cpp
    FtxuiLogView.cpp
    FtxuiTextClipboard.cpp
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON
)

target_link_libraries(FirelandsTui PUBLIC
    FirelandsShared
    ftxui::component
    spdlog::spdlog_header_only
)

target_precompile_headers(FirelandsTui PRIVATE ${PROJECT_PCH_HEADERS})
