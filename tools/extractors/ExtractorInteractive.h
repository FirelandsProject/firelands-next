#pragma once

namespace firelands::extract {

// Console menu: choose task, enter paths, run until user exits.
// Returns process exit code (0 = clean exit from menu).
int RunInteractiveMenu(const char *programName);

} // namespace firelands::extract
