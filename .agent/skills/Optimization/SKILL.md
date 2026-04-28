---
name: build-optimization
description: Optimizes build times using Ninja, ccache, and Precompiled Headers (PCH). Use when adding new targets or heavy dependencies.
---
# Build Optimization & Performance

## Core Principles
To maintain a fast development cycle, the build system is optimized for speed and efficiency. All modifications should respect these optimization patterns.

## Optimization Tools
1. **ccache**: Ensure `ccache` is installed on your system. CMake is configured to automatically detect and use it to cache object files.
2. **Ninja**: Use the Ninja build generator (`cmake -G Ninja`) instead of Make for faster orchestration and better parallelism.
3. **Precompiled Headers (PCH)**: 
   - Heavy headers (STL, spdlog, nlohmann_json) are precompiled.
   - Every new target should apply `${PROJECT_PCH_HEADERS}` or `${TEST_PCH_HEADERS}` using `target_precompile_headers`.

## Build Directive
- **Compile with Ninja:** all builds should be executed using Ninja (i.e., a build directory configured with the Ninja generator). Do not use Make-based builds.

## Build Configurations
1. **Unity Builds**: Grouping multiple source files to reduce preprocessor load. Toggle with `-DENABLE_UNITY_BUILD=ON/OFF`.
2. **Incremental Builds**: Optimization of PCH and ccache usage to keep incremental build times minimal.

## Best Practices
- **Forward Declarations**: Prefer forward declarations in header files to reduce inclusion depth, even with PCH.
- **New Targets**: When adding a new library in `src/`, always update its `CMakeLists.txt` to include:
  ```cmake
  target_precompile_headers(<target_name> PRIVATE ${PROJECT_PCH_HEADERS})
  ```

## Workflow Integration
- **Understand:** Assess the impact of new code on build times and runtime performance.
- **Plan:** Choose appropriate header inclusion methods (forward declarations vs. includes) and ensure new targets use PCH.
- **Implement:** Apply build optimization best practices when creating or modifying targets.
- **Verify (Tests):** Monitor compilation times and run benchmarks if performance-critical code is changed.
- **Verify (Standards):** Verify that all new build targets are correctly configured for PCH and follow unity build compatibility rules.
