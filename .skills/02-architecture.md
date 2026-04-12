# SKILL-002: Hexagonal Architecture (Ports and Adapters)

## Description
This methodology aims to isolate the business logic (Domain) from technical implementation details (Infrastructure).

## Directory Structure Pattern
- `src/domain/`: Entities, Value Objects, Domain Services, and Repository Interfaces (Ports).
- `src/application/`: Use cases and application services.
- `src/infrastructure/`: Database implementations (MySQL), API implementations (REST), and external library wrappers (Adapters).

## Key Rules
1. **Domain Isolation:** The `domain` layer must not import anything from `infrastructure` or `application`.
2. **Ports as Interfaces:** All communication between the Domain/Application and the external world happens through Interfaces (Abstract Classes in C++).
3. **Dependency Injection:** Use DI to provide Adapters to the Application layer at runtime.
4. **Data Privacy:** Domain entities should not be leaked directly to the REST API; use DTOs (Data Transfer Objects).
