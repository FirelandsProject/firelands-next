#ifndef FIRELANDS_SHARED_NETWORK_AUTH_PACKET_H
#define FIRELANDS_SHARED_NETWORK_AUTH_PACKET_H

#include <shared/Common.h>
#include <shared/network/ByteBuffer.h>
#include <shared/network/AuthPackets.h>

namespace Firelands {

    /**
     * @brief Models an Authentication Packet with a 1-byte opcode.
     */
    class AuthPacket : public ByteBuffer {
    public:
        /// @brief Default constructor
        AuthPacket() : ByteBuffer(), _opcode(0) {}

        /**
         * @brief Construct with opcode and initial capacity
         * @param opcode The 1-byte opcode of the authentication packet
         * @param initialCapacity Initial size for the buffer
         */
        AuthPacket(uint8 opcode, size_t initialCapacity = 64) 
            : ByteBuffer(initialCapacity), _opcode(opcode) {}

        /// @brief Gets the packet opcode
        uint8 GetOpcode() const { return _opcode; }

        /// @brief Sets the packet opcode
        void SetOpcode(uint8 opcode) { _opcode = opcode; }

        /**
         * @brief Identifies if the packet is of a specific type
         * @param opcode the opcode to check against
         */
        bool Is(uint8 opcode) const { return _opcode == opcode; }

        /// @brief Gets a human-readable name for the opcode
        std::string GetOpcodeName() const {
            switch (_opcode) {
                case AUTH_LOGON_CHALLENGE: return "AUTH_LOGON_CHALLENGE";
                case AUTH_LOGON_PROOF:     return "AUTH_LOGON_PROOF";
                case AUTH_REALM_LIST:      return "AUTH_REALM_LIST";
                default: return "AUTH_UNKNOWN(0x" + std::to_string(_opcode) + ")";
            }
        }

    private:
        uint8 _opcode;
    };

} // namespace Firelands

#endif // FIRELANDS_SHARED_NETWORK_AUTH_PACKET_H
