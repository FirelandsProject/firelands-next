#include "Config.h"
#include <shared/Logger.h>

#include <cstdlib>
#include <filesystem>
#include <vector>

namespace Firelands {

namespace fs = std::filesystem;

Config &Config::Instance() {
  static Config instance;
  return instance;
}

YAML::Node Config::resolveNestedRead(const YAML::Node &n,
                                     const std::vector<std::string> &keys,
                                     std::size_t i) {
  if (i >= keys.size())
    return n;
  const YAML::Node child = n[keys[i]];
  if (!child)
    return {};
  return resolveNestedRead(child, keys, i + 1);
}

bool Config::LoadFromSearchPaths(const std::string &basename, const char *argv0,
                                 const char *envVarName) {
  Config &cfg = Instance();
  std::vector<fs::path> candidates;

  // Prefer cwd and paths next to the executable first. If FIRELANDS_*_CONFIG
  // is set first, a stale or minimal file can "win" and load with no RealmLink.
  candidates.emplace_back(basename);

  if (argv0) {
    std::error_code ec;
    fs::path raw(argv0);
    fs::path exe = raw.is_absolute()
                       ? fs::weakly_canonical(raw, ec)
                       : fs::weakly_canonical(fs::current_path() / raw, ec);
    if (!ec) {
      fs::path dir = exe.parent_path();
      for (int depth = 0; depth < 8; ++depth) {
        candidates.push_back(dir / basename);
        fs::path parent = dir.parent_path();
        if (parent == dir)
          break;
        dir = std::move(parent);
      }
    }
  }

  if (envVarName) {
    if (const char *ev = std::getenv(envVarName)) {
      if (ev[0] != '\0')
        candidates.emplace_back(ev);
    }
  }

  for (fs::path const &p : candidates) {
    std::error_code ec;
    if (!fs::exists(p, ec))
      continue;
    std::error_code ec2;
    fs::path abs = fs::weakly_canonical(p, ec2);
    std::string const pathStr = ec2 ? p.string() : abs.string();
    if (cfg.Load(pathStr))
      return true;
  }
  return false;
}

bool Config::HasNestedKey(const std::vector<std::string> &keys) const {
  try {
    YAML::Node const tail = resolveNestedRead(_config, keys, 0);
    return tail.IsDefined();
  } catch (...) {
    return false;
  }
}

} // namespace Firelands
