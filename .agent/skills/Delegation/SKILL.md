---
name: strategic-delegation
description: Manages task delegation to specialized virtual agents (Auth, World, DB, Core). Use for complex tasks spanning multiple domains.
---
# Strategic Delegation and Roles

## Specialized Roles
### 1. Auth Architect (`auth-agent`)
- **Scope:** `src/auth/`, SRP6 logic, Realm management, account security.
- **Goal:** Ensure secure and standard-compliant authentication flow.
- **Delegation Trigger:** Any change to authentication protocols, login packets, or account-related infrastructure.

### 2. World Architect (`world-agent`)
- **Scope:** `src/world/`, `src/shared/network/`, Packet handling (Opcodes), Object simulation, Movement.
- **Goal:** Maintain a performant and accurate WoW 4.3.4 world simulation.
- **Delegation Trigger:** Implementing new opcodes, entity systems, or game mechanics.

### 3. Data Engineer (`db-agent`)
- **Scope:** `sql/`, `src/infrastructure/persistence/`, Entity mapping.
- **Goal:** Optimize database schemas and ensure data integrity across Auth, Characters, and World DBs.
- **Delegation Trigger:** SQL migrations, repository implementations, or database performance tuning.

### 4. Core Engineer (`core-agent`)
- **Scope:** `src/domain/`, `src/application/`, `src/shared/`.
- **Goal:** Enforce Hexagonal Architecture (SKILL-002) and TDD (SKILL-003).
- **Delegation Trigger:** Refactoring business logic, adding new Domain entities, or updating Application Use Cases.

## Delegation Protocol
When a Directive is received:
1. **Analyze:** Identify which specialized role is most relevant.
2. **Delegate:** Invoke `generalist` with a prompt that starts by assuming the specific role.
   - *Example:* "As the World Architect, implement the CMSG_MOVE_HEARTBEAT opcode following SKILL-003..."
3. **Verify:** Review the output of the sub-agent against the relevant Project Mandates.

## Workflow Integration
- **Understand:** Evaluate the task scope to identify the primary architectural, domain, or infrastructure focus.
- **Plan:** Determine if specialized sub-agent intervention is needed and select the appropriate role.
- **Implement:** Execute the task delegation via the `generalist` agent, providing clear, context-aware prompts.
- **Verify (Tests):** Verify that the delegated agent has produced valid, tested, and high-quality code.
- **Verify (Standards):** Ensure the resulting work aligns with project conventions for that specific domain/role.
