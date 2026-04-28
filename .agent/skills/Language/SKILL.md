---
name: language-and-naming
description: Ensures all code, comments, and git history use consistent English and technical nomenclature. Use whenever writing code, adding comments, or preparing commit messages.
---
# Language and Naming

## Directives
1. **Total English:** All code components (variables, functions, types, constants) must be named in English.
2. **Technical English:** Use standard technical terms and WoW nomenclature (e.g., `Aura`, `Unit`, `PowerType`, `SpellEffect`).
3. **Comments:** All comments and documentation must be in English.
4. **Git:** Commit messages and branch names must be in English.

## Examples
- **Better:** `uint32_t health = target->GetHealth();`
- **Avoid:** `uint32_t vida = target->ObtenerVida();`

## Workflow Integration
- **Understand:** Review the context to ensure the task aligns with English-only naming and documentation requirements.
- **Plan:** When proposing new naming or documentation, ensure it follows the specified nomenclature and English standards.
- **Implement:** Write code, comments, and commit messages consistently in English.
- **Verify (Tests):** Ensure test descriptions and output are in English.
- **Verify (Standards):** Run linters to check for non-English identifiers if available, and manually review all new code for compliance.
