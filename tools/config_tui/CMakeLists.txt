add_executable(firelands-config
    config_tui_main.cpp
    ConfigEditorFtxui.cpp
)

set_source_files_properties(ConfigEditorFtxui.cpp
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

target_link_libraries(firelands-config PRIVATE
    FirelandsTui
    yaml-cpp
    ftxui::component
)

target_compile_features(firelands-config PRIVATE cxx_std_20)
