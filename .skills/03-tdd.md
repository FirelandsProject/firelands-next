# SKILL-003: Test-Driven Development (TDD)

## Description
Ensure logic correctness through iterative testing.

## Workflow
1. **Red Stage:** Write a unit test that defines a small piece of functionality. Run it and watch it fail.
2. **Green Stage:** Write the simplest code possible to make the test pass.
3. **Refactor Stage:** Clean up the code, improve structure, and ensure it still passes the tests.

## Rules
1. **No Logic without Tests:** Do not write production code without a corresponding test unless it's pure boilerplate.
2. **Unit Tests:** Focus on testing small, isolated components in the Domain layer.
3. **Mocking:** Use mocks/stubs for Ports (Interfaces) to isolate Domain tests from Infrastructure (e.g., mock the Database).
4. **Integration Tests:** test the interaction between Adapters and the Domain in a separate suite.
