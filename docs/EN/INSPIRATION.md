# Inspiration

Firelands Next is born from years of studying and learning from several open-source World of Warcraft server projects that have kept the Cataclysm expansion alive for the community.

Over the years, we have deeply analyzed the source code and architecture of projects such as:

- **TrinityCore** — The most widely used WoW server emulator, known for its extensive documentation and active development community.
- **MaNGOS** — One of the earliest open-source server emulators that pioneered many of the concepts used in modern core implementations.
- **Firelands Core** — A specialized Cataclysm-focused core that contributed valuable insights into the 4.3.4 client version.
- **AzerothCore** — A modern continuation that has pushed forward the state of the art in server emulation.

From these projects, we have learned extensively about:

- Packet structures and opcode handling for the Cataclysm 4.3.4 client
- Database schema design for characters, world, and auth databases
- DBC file formats and parsing for game data (spells, items, creatures)
- World server architecture, map handling, and spatial queries
- Authentication protocols and session management
- Spell system implementation and combat mechanics

## Our Approach

Rather than forking or modifying an existing codebase, Firelands Next was built **from scratch** with a clear architectural vision:

- **Modern C++17** with a focus on type safety and compile-time checks
- **Hexagonal Architecture** to cleanly separate domain logic from infrastructure concerns
- **Clean abstraction layers** that make the codebase maintainable and testable
- **Modular design** allowing for independent development and testing of features

We believe that understanding the existing solutions was essential to building our own. This project pays homage to the open-source community that has kept WoW private servers alive, while charting its own path toward a cleaner, more maintainable implementation.

---

*This document serves as acknowledgment of the open-source projects that inspired Firelands Next. We are grateful to the communities behind TrinityCore, MaNGOS, Firelands Core, and AzerothCore for their contributions to the emulator ecosystem.*