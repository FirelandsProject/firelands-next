# Inspiration

Firelands Next targets World of Warcraft Cataclysm **4.3.4** (client build **15595**).

Design and wire layouts are validated against:

- Local reference clone **`firelands-cata-ref/`** (game logic and SQL for this build)
- Client **DBC/DB2** files and **WowPacketParser** definitions for 15595
- Public WoW data references (e.g. wowdev) where applicable

## Our Approach

Rather than forking an existing codebase, Firelands Next was built **from scratch** with a clear architectural vision:

- **Modern C++20** with a focus on type safety and compile-time checks
- **Hexagonal Architecture** to cleanly separate domain logic from infrastructure concerns
- **Clean abstraction layers** that make the codebase maintainable and testable
- **Modular design** allowing for independent development and testing of features

We study reference material to match client behavior, while keeping implementation and structure owned by this project.
