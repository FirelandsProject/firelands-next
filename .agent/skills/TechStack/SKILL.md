---
name: tech-stack
description: Defines C++17, CMake, and MySQL standards for cross-platform support. Use to ensure compatibility across Windows, Linux, and macOS.
---
# Technical Stack & Platform Support

## Stack Definition
- **Language:** C++17 (Use `<filesystem>`, `std::optional`, `std::variant`, etc.).
- **Build Tool:** CMake 3.10+.
- **Database:** MySql 8.0+.
- **API:** RESTful (JSON).

## Multi-Platform Directives
1. **CMake Abstraction:** Use CMake to manage include paths, library links, and compiler flags for each platform.
2. **Standard Library:** Favor `std::` over platform-specific libraries (e.g., use `std::thread` instead of `pthreads` or Windows Threads).
3. **Path Handling:** Use `std::filesystem` for cross-platform path manipulation.
4. **Conditional Compilation:** Use C++ macros only when absolutely necessary for platform-specific system calls, and keep them hidden inside Adapters.

## Verification
- Code must build on:
    - **Windows:** MSVC or MinGW.
    - **Linux:** GCC or Clang.
    - **macOS:** Clang (Apple Clang).

## Workflow Integration
- **Understand:** Review the required tech stack and platform constraints before starting the task.
- **Plan:** Ensure the proposed solution utilizes C++17 standard features and cross-platform CMake practices.
- **Implement:** Write code adhering to the language and platform-abstraction guidelines.
- **Verify (Tests):** Ensure tests are compatible with the cross-platform nature of the codebase.
- **Verify (Standards):** Build the project on the relevant target platforms (or simulate as appropriate) and ensure strict adherence to C++17 and standard library usage.
