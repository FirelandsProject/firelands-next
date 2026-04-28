---
name: hexagonal-architecture
description: Enforces Hexagonal Architecture (Ports and Adapters) to isolate domain logic from infrastructure. Use when adding new features, services, or repository implementations.
---
# Hexagonal Architecture (Ports and Adapters)

## Directory Structure Pattern
- `src/domain/`: Entities, Value Objects, Domain Services, and Repository Interfaces (Ports).
- `src/application/`: Use cases and application services.
- `src/infrastructure/`: Database implementations (MySQL), API implementations (REST), and external library wrappers (Adapters).

## Key Rules
1. **Domain Isolation:** The `domain` layer must not import anything from `infrastructure` or `application`.
2. **Ports as Interfaces:** All communication between the Domain/Application and the external world happens through Interfaces (Abstract Classes in C++).
3. **Dependency Injection:** Use DI to provide Adapters to the Application layer at runtime.
4. **Data Privacy:** Domain entities should not be leaked directly to the REST API; use DTOs (Data Transfer Objects).

## Workflow
1. **Identify the Use Case:** Describe the user intent (e.g., “create character”, “load character list”, “update movement state”).
2. **Model in Domain:** Add/extend entities, value objects, and domain services without any knowledge of DB, sockets, JSON, or frameworks.
3. **Define Ports (Interfaces):** Add repository/service interfaces in `src/domain/repositories/` (and similar “ports” locations) that express what the domain/application needs.
4. **Implement Application Orchestration:** Implement the use case in `src/application/` using ports and domain types, coordinating transactions and cross-aggregate logic.
5. **Write Adapters:** Implement ports in `src/infrastructure/` (MySQL repositories, network/session handlers, REST controllers, etc.).
6. **Wire via DI/Factories:** Bind the concrete adapters to the ports at runtime (composition root), keeping constructors/test seams clean.

## Port & Adapter Checklist
- **Port shape**: Interface methods use domain types (IDs/value objects) rather than infrastructure DTOs or DB row structs.
- **Error boundaries**: Domain/application errors are expressed in domain language (enums/variants), not DB/network error codes.
- **No leakage**: Infrastructure headers (`mysql`, `asio`, `json`, etc.) never appear in domain headers.
- **Tests**: Domain/application logic can be unit-tested by mocking ports; adapter behavior is covered by integration tests.

## Workflow Integration
- **Understand:** Review existing domain models and infrastructure adapters to determine where the new functionality fits within the layered architecture.
- **Plan:** Define the necessary Port (Interface) in `domain/` and outline the corresponding Adapter in `infrastructure/`.
- **Implement:** Write the Domain logic, then the Application Service, and finally the Infrastructure Adapter. Ensure strict adherence to dependency direction.
- **Verify (Tests):** Verify domain logic with unit tests (using mocks for infrastructure) and verify adapter functionality with integration tests.
- **Verify (Standards):** Check for proper dependency inversion and ensuring no domain-to-infrastructure imports exist.

## Verification
- **Dependency direction**: `domain` compiles with no includes from `application`/`infrastructure`.
- **Headers**: Domain headers only include standard library and other domain headers.
- **Composition root**: All concrete adapter creation is centralized (no `new MySql...` inside domain).
- **Diff sanity**: New features add ports first, then adapters; refactors don’t invert dependencies.
