#pragma once

/// Full-screen TUI to edit authserver.yaml and worldserver.yaml (paths resolved
/// like the servers: cwd, parents of argv[0], then FIRELANDS_*_CONFIG).

int RunConfigEditorFtxui(int argc, char **argv);
