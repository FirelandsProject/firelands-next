# Roadmap (EN)

The roadmap currently lives in Spanish under [docs/ES/ROADMAP.md](../ES/ROADMAP.md) and is the **single source of truth**.

If you prefer, we can later translate the full roadmap into English and keep both versions in sync.

## Workspace snapshot (doc update 2026-05-03)

- **In flight:** `WorldSession` split into `worldsession/*.cpp` (login flow, client opcode handlers, outbound sends).
- **Shared packet layer:** new headers under `src/shared/network/packets/` (client read structs, 4.3.4 packed player `ObjectGuid`, small server `ServerPacket` helpers).
- **Next priorities (see Spanish roadmap):** finish this refactor (build + commit), then spell auras / minimal spell data, plus client idle stability (≥5 min) and time-sync behaviour.

