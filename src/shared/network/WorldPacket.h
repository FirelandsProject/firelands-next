#ifndef FIRELANDS_SHARED_NETWORK_WORLD_PACKET_H
#define FIRELANDS_SHARED_NETWORK_WORLD_PACKET_H

#include <shared/Common.h>
#include <shared/network/ByteBuffer.h>
#include <shared/network/WorldOpcodes.h>

namespace Firelands {

    /**
     * @brief Models a World Packet with an opcode and payload.
     */
    class WorldPacket : public ByteBuffer {
    public:
        /// @brief Default constructor
        WorldPacket() : ByteBuffer(), _opcode(0) {}

        /**
         * @brief Construct with opcode and initial capacity
         * @param opcode The opcode of the packet
         * @param initialCapacity Initial size for the buffer
         */
        WorldPacket(uint32 opcode, size_t initialCapacity = 200) 
            : ByteBuffer(initialCapacity), _opcode(opcode) {}

        /// @brief Gets the packet opcode
        uint32 GetOpcode() const { return _opcode; }

        /// @brief Sets the packet opcode
        void SetOpcode(uint32 opcode) { _opcode = opcode; }

        /**
         * @brief Identifies if the packet is of a specific type
         * @param opcode the opcode to check against
         */
        bool Is(uint32 opcode) const { return _opcode == opcode; }

        /**
         * @brief helper to check if it's a client or server opcode
         * (In Cataclysm, client opcodes often have different ranges or structures, 
         * but for now we just use the raw opcode)
         */
        std::string GetOpcodeName() const {
            // This could eventually use a map to return "CMSG_AUTH_SESSION" etc.
            return "Opcode 0x" + std::to_string(_opcode); 
        }

    private:
        uint32 _opcode;
    };

} // namespace Firelands

#endif // FIRELANDS_SHARED_NETWORK_WORLD_PACKET_H
