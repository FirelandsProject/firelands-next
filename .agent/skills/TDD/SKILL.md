---
name: test-driven-development
description: Ensures logic correctness via Red-Green-Refactor cycle. Use for all logic changes to maintain project integrity.
---
# Test-Driven Development (TDD)

## Workflow
1. **Red Stage:** Write a unit test that defines a small piece of functionality. Run it and watch it fail.
2. **Green Stage:** Write the simplest code possible to make the test pass.
3. **Refactor Stage:** Clean up the code, improve structure, and ensure it still passes the tests.

## Rules
1. **No Logic without Tests:** Do not write production code without a corresponding test unless it's pure boilerplate.
2. **Unit Tests:** Focus on testing small, isolated components in the Domain layer.
3. **Mocking:** Use mocks/stubs for Ports (Interfaces) to isolate Domain tests from Infrastructure (e.g., mock the Database).
4. **Integration Tests:** Test the interaction between Adapters and the Domain in a separate suite.

## Workflow Integration
- **Understand:** Define the specific functionality to be tested before writing any production code.
- **Plan:** Outline the test case(s) required for the feature, following the Red-Green-Refactor cycle.
- **Implement:** Write the failing test first, then the simplest implementation, followed by refactoring.
- **Verify (Tests):** Execute the specific unit/integration test suite to confirm the Red-Green-Refactor progress.
- **Verify (Standards):** Ensure full coverage for new logic and that all tests pass before completing the task.
