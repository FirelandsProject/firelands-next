# Skills Index (Cursor)

This folder contains project “skills” used by Cursor to enforce consistent engineering practices.

## How to use
- Pick the skill that best matches the task’s primary concern (architecture, tests, build, etc.).
- If a task spans multiple concerns, apply the **primary** skill first, then layer in secondary skills as needed.

## Quick chooser
- If you want the **global baseline for all work** → `Mandates/SKILL.md`
- If you are **writing/renaming code, comments, or commit messages** → `Language/SKILL.md`
- If you are **adding features / services / repositories** or doing **cross-layer refactors** → `Architecture/SKILL.md`
- If you are **changing business logic** (domain/app) → `TDD/SKILL.md` (and usually `Architecture/SKILL.md`)
- If you are **touching CMake, targets, headers, build times** → `Optimization/SKILL.md` (and `TechStack/SKILL.md`)
- If you are **working with platform constraints** (Windows/Linux/macOS) or toolchain choices → `TechStack/SKILL.md`
- If the work is **large and spans multiple areas** (auth/world/db/core) → `Delegation/SKILL.md`

## Skills (what each one is for)
- **Project Mandates (baseline)** (`Mandates/SKILL.md`)
  - Global non-negotiables (language, reference source, delegation, communication style, rebuild policy).
- **Language and Naming** (`Language/SKILL.md`)
  - Enforces English-only naming, technical nomenclature, and consistent git history language.
- **Hexagonal Architecture** (`Architecture/SKILL.md`)
  - Keeps `domain` isolated, introduces ports first, implements adapters in `infrastructure`, and wires via DI.
- **TDD** (`TDD/SKILL.md`)
  - Red/Green/Refactor for logic changes; mock ports for unit tests; integration tests for adapters.
- **Technical Stack & Platform Support** (`TechStack/SKILL.md`)
  - C++17/CMake/MySQL expectations and cross-platform directives.
- **Build Optimization & Performance** (`Optimization/SKILL.md`)
  - ccache/Ninja/PCH and build best practices.
- **Strategic Delegation and Roles** (`Delegation/SKILL.md`)
  - How to split complex tasks into specialized roles (auth/world/db/core) and validate outputs.

## Common combinations
- **New use case (feature)**: `Architecture` + `TDD` (+ `Language`)
- **New MySQL repository / schema change**: `Architecture` + `TechStack` (+ `Optimization` if build touched)
- **Opcode / session / packet work**: `Architecture` (+ `TDD` where testable) + consider `Delegation`
- **Build regression or heavy dependency added**: `Optimization` + `TechStack`

## Maintenance rule
Avoid duplicating rules across skills. Prefer:
- Putting stable, cross-cutting rules in the most appropriate skill (often `Language` or `TechStack`), and
- Referencing that skill from others via this index.

