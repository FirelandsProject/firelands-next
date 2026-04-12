#ifndef FIRELANDS_APPLICATION_PORTS_I_NETWORK_SERVER_H
#define FIRELANDS_APPLICATION_PORTS_I_NETWORK_SERVER_H

#include <shared/Common.h>
#include <string>

namespace Firelands {

    class INetworkServer {
    public:
        virtual ~INetworkServer() = default;
        
        virtual bool Start(const std::string& address, uint16 port) = 0;
        virtual void Stop() = 0;
        virtual void Update() = 0;
    };

} // namespace Firelands

#endif // FIRELANDS_APPLICATION_PORTS_I_NETWORK_SERVER_H
