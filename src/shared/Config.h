#ifndef FIRELANDS_SHARED_CONFIG_H
#define FIRELANDS_SHARED_CONFIG_H

#include <string>
#include <yaml-cpp/yaml.h>
#include <optional>
#include <shared/Logger.h>
#include <cctype>
#include <vector>

namespace YAML {
    template<>
    struct convert<Firelands::LogLevel> {
        static Node encode(const Firelands::LogLevel& rhs) {
            Node node;
            switch (rhs) {
                case Firelands::LogLevel::Trace: node = "trace"; break;
                case Firelands::LogLevel::Debug: node = "debug"; break;
                case Firelands::LogLevel::Info: node = "info"; break;
                case Firelands::LogLevel::Warn: node = "warn"; break;
                case Firelands::LogLevel::Error: node = "error"; break;
                case Firelands::LogLevel::Critical: node = "critical"; break;
                case Firelands::LogLevel::Off: node = "off"; break;
                default: node = "info"; break;
            }
            return node;
        }

        static bool decode(const Node& node, Firelands::LogLevel& rhs) {
            if (!node.IsScalar()) {
                return false;
            }

            std::string val = node.as<std::string>();
            for (auto& c : val) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }

            if (val == "trace") rhs = Firelands::LogLevel::Trace;
            else if (val == "debug") rhs = Firelands::LogLevel::Debug;
            else if (val == "info") rhs = Firelands::LogLevel::Info;
            else if (val == "warn" || val == "warning") rhs = Firelands::LogLevel::Warn;
            else if (val == "error") rhs = Firelands::LogLevel::Error;
            else if (val == "critical") rhs = Firelands::LogLevel::Critical;
            else if (val == "off") rhs = Firelands::LogLevel::Off;
            else return false;

            return true;
        }
    };
}

namespace Firelands {

    class Config {
    public:
        static Config &Instance();

        bool Load(const std::string& filename) {
            try {
                _config = YAML::LoadFile(filename);
                _filename = filename;
                if (Logger::IsInitialized()) {
                    LOG_INFO("Config loaded: {}", filename);
                }
                return true;
            } catch (const std::exception& e) {
                _filename.clear();
                if (Logger::IsInitialized()) {
                    LOG_ERROR("Failed to load config {}: {}", filename, e.what());
                }
                return false;
            }
        }

        /// Tries `basename` in cwd, then next to `argv0` and parent dirs, then
        /// `getenv(envVarName)` (env last so stale FIRELANDS_* does not shadow the
        /// project yaml).
        static bool LoadFromSearchPaths(const std::string &basename,
                                        const char *argv0,
                                        const char *envVarName);

        /// Path passed to the last successful `Load`, or empty.
        const std::string &GetLoadedConfigPath() const { return _filename; }

        /// True if every key segment exists (final node may be null).
        bool HasNestedKey(const std::vector<std::string> &keys) const;

        template<typename T>
        T Get(const std::string& key, T defaultValue) const {
            try {
                if (_config[key]) {
                    return _config[key].as<T>();
                }
            } catch (...) {}
            return defaultValue;
        }

        template<typename T>
        std::optional<T> Get(const std::string& key) const {
            try {
                if (_config[key]) {
                    return _config[key].as<T>();
                }
            } catch (...) {}
            return std::nullopt;
        }

        // Support for nested keys like "Database.Auth.Host"
        template<typename T>
        T GetNested(const std::vector<std::string>& keys, T defaultValue) const {
            try {
                YAML::Node const located = resolveNestedRead(_config, keys, 0);
                if (!located) return defaultValue;
                return located.as<T>();
            } catch (...) {
                return defaultValue;
            }
        }

        /// Scalar string at nested path using YAML’s raw scalar text (avoids
        /// `as<std::string>()` failing on ambiguous or very long hex-like tokens).
        std::string GetNestedScalarString(const std::vector<std::string>& keys,
                                          const std::string& defaultValue) const {
            try {
                YAML::Node const located = resolveNestedRead(_config, keys, 0);
                if (!located.IsDefined())
                    return defaultValue;
                if (located.IsScalar())
                    return located.Scalar();
                return located.as<std::string>();
            } catch (...) {
                return defaultValue;
            }
        }

    private:
        Config() = default;
        /// Read-only nested lookup (always uses const YAML::Node indexing so the
        /// document is never mutated; non-const `operator[]` on a copied Node can
        /// insert keys and hide real map entries in yaml-cpp).
        static YAML::Node resolveNestedRead(const YAML::Node &n,
                                          const std::vector<std::string> &keys,
                                          std::size_t i);
        YAML::Node _config;
        std::string _filename;
    };

} // namespace Firelands

#endif // FIRELANDS_SHARED_CONFIG_H
