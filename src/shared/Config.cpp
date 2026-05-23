#include "Config.h"
#include <shared/Logger.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <vector>

namespace Firelands {

namespace fs = std::filesystem;

std::vector<fs::path> Config::s_dataSearchRoots;

namespace {

void TrimAsciiInPlace(std::string &s) {
  auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
  s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
}

bool ParseBoolScalar(const std::string &raw, bool defaultValue) {
  std::string s = raw;
  TrimAsciiInPlace(s);
  for (char &c : s) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}
  if (s.empty())
    return defaultValue;
  if (s == "true" || s == "1" || s == "yes" || s == "on")
    return true;
  if (s == "false" || s == "0" || s == "no" || s == "off")
    return false;
    return defaultValue;
}

} // namespace

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

    s_dataSearchRoots.clear();
    s_dataSearchRoots.emplace_back(fs::current_path());

  for (fs::path const &p : candidates) {
    std::error_code ec;
    if (!fs::exists(p, ec))
      continue;
    std::error_code ec2;
    fs::path abs = fs::weakly_canonical(p, ec2);
    std::string const pathStr = ec2 ? p.string() : abs.string();
        if (cfg.Load(pathStr)) {
            fs::path const configDir = abs.parent_path();
            if (!configDir.empty())
                s_dataSearchRoots.push_back(configDir);
  if (argv0) {
    fs::path raw(argv0);
    fs::path exe = raw.is_absolute()
                       ? fs::weakly_canonical(raw, ec)
                       : fs::weakly_canonical(fs::current_path() / raw, ec);
    if (!ec) {
      fs::path dir = exe.parent_path();
      for (int depth = 0; depth < 8; ++depth) {
                        s_dataSearchRoots.push_back(dir);
        fs::path parent = dir.parent_path();
        if (parent == dir)
          break;
        dir = std::move(parent);
}
}
}
    return true;
}
}
    return false;
}

std::string Config::ResolveDataDirectory(const std::string &relativePath) {
    if (relativePath.empty())
        return relativePath;

    std::error_code ec;
    fs::path const rel(relativePath);
    auto tryCandidate = [&](fs::path const &base) -> std::optional<std::string> {
        fs::path const dir = base.empty() ? rel : (base / rel);
        if (!fs::exists(dir / "Spell.dbc", ec))
            return std::nullopt;
    std::error_code ec2;
        fs::path const canon = fs::weakly_canonical(dir, ec2);
        return ec2 ? dir.string() : canon.string();
    };

    if (rel.is_absolute()) {
        if (fs::exists(rel / "Spell.dbc", ec)) {
    std::error_code ec2;
            fs::path const canon = fs::weakly_canonical(rel, ec2);
            return ec2 ? relativePath : canon.string();
}
        return relativePath;
}

    for (fs::path const &root : s_dataSearchRoots) {
        if (auto resolved = tryCandidate(root))
            return *resolved;
}

    if (Logger::IsInitialized()) {
        LOG_WARN(
                "Could not resolve Data directory '{}' (Spell.dbc not found under cwd, "
                "config dir, or executable parents); spell costs may be zero.",
                relativePath);
}
    return relativePath;
}

bool Config::HasNestedKey(const std::vector<std::string> &keys) const {
  try {
    YAML::Node const tail = resolveNestedRead(_config, keys, 0);
    return tail.IsDefined();
  } catch (...) {
    return false;
}
}

bool Config::GetNestedBool(const std::vector<std::string> &keys,
                           bool defaultValue) const {
  try {
    YAML::Node const located = resolveNestedRead(_config, keys, 0);
    if (!located || !located.IsDefined())
    return defaultValue;
    if (located.IsScalar())
      return ParseBoolScalar(located.Scalar(), defaultValue);
    return located.as<bool>();
  } catch (...) {
    return defaultValue;
}
}

} // namespace Firelands
