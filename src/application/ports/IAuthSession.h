#ifndef FIRELANDS_APPLICATION_PORTS_I_AUTH_SESSION_H
#define FIRELANDS_APPLICATION_PORTS_I_AUTH_SESSION_H

#include <shared/network/ByteBuffer.h>
#include <memory>

namespace Firelands {

    class IAuthSession {
    public:
        virtual ~IAuthSession() = default;
        
        virtual void SendPacket(ByteBuffer& buffer) = 0;
        virtual void Close() = 0;
        virtual std::string GetIpAddress() const = 0;
    };

} // namespace Firelands

#endif // FIRELANDS_APPLICATION_PORTS_I_AUTH_SESSION_H
