#ifndef FIRELANDS_INFRASTRUCTURE_PERSISTENCE_DATABASE_SERVICE_H
#define FIRELANDS_INFRASTRUCTURE_PERSISTENCE_DATABASE_SERVICE_H

#include <conncpp.hpp>
#include <memory>
#include <string>

namespace Firelands {

    class DatabaseService {
    public:
        DatabaseService(const std::string& url, const std::string& user, const std::string& pass)
            : _url(url), _user(user), _pass(pass) {}

        std::shared_ptr<sql::Connection> CreateConnection() {
            sql::Properties properties({
                {"user", _user},
                {"password", _pass}
            });
            
            sql::Driver* driver = sql::mariadb::get_driver_instance();
            return std::shared_ptr<sql::Connection>(driver->connect(_url, properties));
        }

    private:
        std::string _url;
        std::string _user;
        std::string _pass;
    };

} // namespace Firelands

#endif // FIRELANDS_INFRASTRUCTURE_PERSISTENCE_DATABASE_SERVICE_H
