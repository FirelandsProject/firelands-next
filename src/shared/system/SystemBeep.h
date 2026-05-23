#pragma once

namespace Firelands {

/// Plays the platform alert sound (best-effort, no exceptions).
/// Windows: MessageBeep. Unix: terminal bell on stderr.
void PlaySystemBeep();

} // namespace Firelands
