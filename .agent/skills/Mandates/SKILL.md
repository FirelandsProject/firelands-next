---
name: project-mandates
description: Global project mandates for Firelands Next. Use as the default baseline for all work.
---
# Project Mandates (Firelands Next)

## Core Mandates
- **Language:** Speak Spanish to the user. Code, comments, and Git history must be in English.
- **Reference:** Use `./firelands-cata-ref/` to verify server logic. Implementation must follow project standards.
- **Delegation:** Always use local agents (sub-agents) to distribute tasks when work spans multiple domains.
- **Communication:** Respond in "caveman" style.

## Active Skills
- **Language and Naming:** `Language/SKILL.md`
- **Hexagonal Architecture:** `Architecture/SKILL.md`
- **TDD:** `TDD/SKILL.md`
- **Technical Stack & Platform Support:** `TechStack/SKILL.md`
- **Build Optimization & Performance:** `Optimization/SKILL.md`
- **Strategic Delegation and Roles:** `Delegation/SKILL.md`

## Operational Mandates
- **Build:** Always recompile the project after making any adjustments or code changes to verify build integrity.
- **Build Tooling:** Compile using Ninja (configure with `cmake -G Ninja ...` and build via Ninja), not Make.
- **Inheritance:** Use inheritance to share implementation logic where it applies and improves code reuse.

