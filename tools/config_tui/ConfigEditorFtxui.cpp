#include "ConfigEditorFtxui.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cctype>
#include <climits>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#ifndef _WIN32
#include <unistd.h>
#else
#include <cstdio>
#include <io.h>
#endif

namespace {

namespace fs = std::filesystem;
using namespace ftxui;

Color const kShellBg = Color::RGB(26, 24, 22);
Color const kAccent = Color::RGB(180, 72, 52);

Component VerticalGap(int lines) {
  int const h = std::max(1, lines);
  return Renderer([h] { return text("") | size(HEIGHT, EQUAL, h); });
}

Component HSpace(int cols) {
  int const w = std::max(1, cols);
  return Renderer([w] {
    return text(std::string(static_cast<std::size_t>(w), ' '));
  });
}

Component JoinVerticalSpaced(std::vector<Component> parts, int gapLines) {
  if (parts.empty()) {
    return Renderer([] { return text(""); });
  }
  int const gap = std::max(1, gapLines);
  Components children;
  for (std::size_t i = 0; i < parts.size(); ++i) {
    if (i > 0) {
      children.push_back(VerticalGap(gap));
    }
    children.push_back(std::move(parts[i]));
  }
  return Container::Vertical(std::move(children));
}

/// Label stack for one setting: human title, YAML key path, optional help, then
/// the interactive control (Input / Checkbox).
Component NestedField(std::string const &yamlPath, std::string const &title,
                      std::string const &help, Component control) {
  return Renderer(control, [yamlPath, title, help, control] {
    Elements lines;
    lines.push_back(text(title) | bold | color(Color::RGB(224, 216, 206)));
    lines.push_back(hbox({
        text("Key ") | dim,
        text(yamlPath) | dim | color(Color::RGB(130, 185, 215)),
    }));
    if (!help.empty()) {
      lines.push_back(text(help) | dim | color(Color::RGB(158, 150, 140)));
    }
    lines.push_back(text("") | size(HEIGHT, EQUAL, 1));
    lines.push_back(control->Render());
    return vbox(std::move(lines));
  });
}

struct IgnoreSigIntForTui {
#ifndef _WIN32
  void (*previous_)(int) = SIG_ERR;
  IgnoreSigIntForTui() { previous_ = ::signal(SIGINT, SIG_IGN); }
  ~IgnoreSigIntForTui() {
    if (previous_ != SIG_ERR) {
      ::signal(SIGINT, previous_);
    }
  }
#else
  IgnoreSigIntForTui() = default;
  ~IgnoreSigIntForTui() = default;
#endif
};

bool StdoutIsInteractiveTerminal() {
#ifndef _WIN32
  return ::isatty(STDOUT_FILENO) != 0;
#else
  return ::_isatty(::_fileno(stdout)) != 0;
#endif
}

std::optional<std::string> ResolveConfigPath(std::string const &basename,
                                             char const *argv0,
                                             char const *envVarName) {
  std::vector<fs::path> candidates;
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
        if (parent == dir) {
          break;
        }
        dir = std::move(parent);
      }
    }
  }

  if (envVarName) {
    if (char const *ev = std::getenv(envVarName)) {
      if (ev[0] != '\0') {
        candidates.emplace_back(ev);
      }
    }
  }

  for (fs::path const &p : candidates) {
    std::error_code ec;
    if (!fs::exists(p, ec)) {
      continue;
    }
    std::error_code ec2;
    fs::path abs = fs::weakly_canonical(p, ec2);
    return ec2 ? p.string() : abs.string();
  }
  return std::nullopt;
}

YAML::Node LoadYamlFileOrEmpty(std::string const &path, std::string &errOut) {
  errOut.clear();
  if (path.empty()) {
    errOut = "Path is empty.";
    return YAML::Node(YAML::NodeType::Map);
  }
  std::error_code ec;
  if (!fs::exists(path, ec)) {
    return YAML::Node(YAML::NodeType::Map);
  }
  try {
    return YAML::LoadFile(path);
  } catch (std::exception const &e) {
    errOut = e.what();
    return YAML::Node(YAML::NodeType::Map);
  }
}

YAML::Node const ResolveRead(YAML::Node const &n,
                             std::vector<std::string> const &keys,
                             std::size_t i) {
  if (i >= keys.size()) {
    return n;
  }
  YAML::Node const child = n[keys[i]];
  if (!child) {
    return {};
  }
  return ResolveRead(child, keys, i + 1);
}

std::string GetScalar(YAML::Node const &root,
                      std::vector<std::string> const &keys,
                      std::string const &def) {
  try {
    YAML::Node const loc = ResolveRead(root, keys, 0);
    if (!loc || !loc.IsDefined()) {
      return def;
    }
    if (loc.IsScalar()) {
      return loc.Scalar();
    }
    return loc.as<std::string>();
  } catch (...) {
    return def;
  }
}

bool GetBool(YAML::Node const &root, std::vector<std::string> const &keys,
             bool def) {
  try {
    YAML::Node const loc = ResolveRead(root, keys, 0);
    if (!loc || !loc.IsDefined()) {
      return def;
    }
    if (loc.IsScalar()) {
      std::string s = loc.Scalar();
      for (char &c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      }
      if (s == "true" || s == "1" || s == "yes" || s == "on") {
        return true;
      }
      if (s == "false" || s == "0" || s == "no" || s == "off") {
        return false;
      }
      return def;
    }
    return loc.as<bool>();
  } catch (...) {
    return def;
  }
}

int GetInt(YAML::Node const &root, std::vector<std::string> const &keys,
           int def) {
  try {
    YAML::Node const loc = ResolveRead(root, keys, 0);
    if (!loc || !loc.IsDefined()) {
      return def;
    }
    return loc.as<int>();
  } catch (...) {
    return def;
  }
}

void TrimInPlace(std::string &s) {
  auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
  s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
}

bool ParseIntStrict(std::string const &s, int &out) {
  std::string t = s;
  TrimInPlace(t);
  if (t.empty()) {
    return false;
  }
  char *end = nullptr;
  long v = std::strtol(t.c_str(), &end, 10);
  if (end != t.c_str() + t.size()) {
    return false;
  }
  if (v < INT_MIN || v > INT_MAX) {
    return false;
  }
  out = static_cast<int>(v);
  return true;
}

std::string MotdFromYaml(YAML::Node const &root) {
  YAML::Node const m = root["Motd"];
  if (!m || !m.IsSequence()) {
    return {};
  }
  std::ostringstream os;
  for (YAML::Node const &item : m) {
    if (!item.IsDefined()) {
      continue;
    }
    std::string line = item.IsScalar() ? item.Scalar() : item.as<std::string>();
    os << line << '\n';
  }
  std::string s = os.str();
  while (!s.empty() && s.back() == '\n') {
    s.pop_back();
  }
  return s;
}

void MotdToYaml(YAML::Node &root, std::string const &multiline) {
  YAML::Node seq(YAML::NodeType::Sequence);
  std::istringstream in(multiline);
  std::string line;
  while (std::getline(in, line)) {
    TrimInPlace(line);
    if (!line.empty()) {
      seq.push_back(line);
    }
  }
  root["Motd"] = seq;
}

struct AuthFields {
  std::string net_bind = "0.0.0.0";
  std::string net_port = "3724";
  std::string net_rest_port = "8081";
  bool console_enabled = true;
  bool console_tui = true;
  bool realm_link_enabled = true;
  std::string realm_link_bind = "127.0.0.1";
  std::string realm_link_port = "3725";
  std::string realm_link_token;
  bool hide_realm_suffix = false;
  std::string db_user = "firelands";
  std::string db_password = "firelands";
  std::string db_auth_uri = "jdbc:mariadb://localhost:3306/firelands_auth";
  std::string db_chars_uri =
      "jdbc:mariadb://localhost:3306/firelands_characters";
  std::string log_level = "Info";
  std::string log_file = "logs/firelands-auth.log";
  bool log_sticky = true;
};

struct WorldFields {
  std::string net_bind = "0.0.0.0";
  std::string net_port = "8085";
  bool console_enabled = true;
  bool console_tui = true;
  bool realm_link_enabled = true;
  std::string realm_auth_host = "127.0.0.1";
  std::string realm_auth_port = "3725";
  std::string realm_id = "1";
  std::string realm_token;
  std::string realm_name = "Firelands";
  std::string scripts_dir = "scripts/lua";
  std::string db_user = "firelands";
  std::string db_password = "firelands";
  std::string db_auth_uri = "jdbc:mariadb://127.0.0.1:3306/firelands_auth";
  std::string db_chars_uri =
      "jdbc:mariadb://127.0.0.1:3306/firelands_characters";
  std::string db_world_uri = "jdbc:mariadb://127.0.0.1:3306/firelands_world";
  std::string log_level = "Debug";
  std::string log_file = "logs/firelands-world.log";
  bool log_sticky = true;
  std::string dbc_path = "data/dbc";
  std::string motd;
};

void YamlToAuth(YAML::Node const &n, AuthFields &f) {
  f.net_bind = GetScalar(n, {"Network", "BindAddress"}, f.net_bind);
  f.net_port = std::to_string(GetInt(n, {"Network", "Port"}, 3724));
  f.net_rest_port = std::to_string(GetInt(n, {"Network", "RestPort"}, 8081));
  f.console_enabled = GetBool(n, {"Console", "Enabled"}, f.console_enabled);
  f.console_tui = GetBool(n, {"Console", "Tui"}, f.console_tui);
  f.realm_link_enabled =
      GetBool(n, {"RealmLink", "Enabled"}, f.realm_link_enabled);
  f.realm_link_bind =
      GetScalar(n, {"RealmLink", "BindAddress"}, f.realm_link_bind);
  f.realm_link_port =
      std::to_string(GetInt(n, {"RealmLink", "Port"}, 3725));
  f.realm_link_token = GetScalar(n, {"RealmLink", "Token"}, f.realm_link_token);
  f.hide_realm_suffix =
      GetBool(n, {"RealmList", "HideRealmSuffixInAuthList"}, false);
  f.db_user = GetScalar(n, {"Database", "User"}, f.db_user);
  f.db_password = GetScalar(n, {"Database", "Password"}, f.db_password);
  f.db_auth_uri = GetScalar(n, {"Database", "Auth", "URI"}, f.db_auth_uri);
  f.db_chars_uri =
      GetScalar(n, {"Database", "Characters", "URI"}, f.db_chars_uri);
  f.log_level = GetScalar(n, {"Log", "Level"}, f.log_level);
  f.log_file = GetScalar(n, {"Log", "File"}, f.log_file);
  f.log_sticky = GetBool(n, {"Log", "StickyBanner"}, f.log_sticky);
}

void YamlToWorld(YAML::Node const &n, WorldFields &f) {
  f.net_bind = GetScalar(n, {"Network", "BindAddress"}, f.net_bind);
  f.net_port = std::to_string(GetInt(n, {"Network", "Port"}, 8085));
  f.console_enabled = GetBool(n, {"Console", "Enabled"}, f.console_enabled);
  f.console_tui = GetBool(n, {"Console", "Tui"}, f.console_tui);
  f.realm_link_enabled =
      GetBool(n, {"RealmLink", "Enabled"}, f.realm_link_enabled);
  f.realm_auth_host = GetScalar(n, {"RealmLink", "AuthHost"}, f.realm_auth_host);
  f.realm_auth_port =
      std::to_string(GetInt(n, {"RealmLink", "AuthPort"}, 3725));
  f.realm_id = std::to_string(GetInt(n, {"RealmLink", "RealmId"}, 1));
  f.realm_token = GetScalar(n, {"RealmLink", "Token"}, f.realm_token);
  f.realm_name = GetScalar(n, {"World", "RealmName"}, f.realm_name);
  f.scripts_dir =
      GetScalar(n, {"Scripting", "ScriptsDirectory"}, f.scripts_dir);
  f.db_user = GetScalar(n, {"Database", "User"}, f.db_user);
  f.db_password = GetScalar(n, {"Database", "Password"}, f.db_password);
  f.db_auth_uri = GetScalar(n, {"Database", "Auth", "URI"}, f.db_auth_uri);
  f.db_chars_uri =
      GetScalar(n, {"Database", "Characters", "URI"}, f.db_chars_uri);
  f.db_world_uri = GetScalar(n, {"Database", "World", "URI"}, f.db_world_uri);
  f.log_level = GetScalar(n, {"Log", "Level"}, f.log_level);
  f.log_file = GetScalar(n, {"Log", "File"}, f.log_file);
  f.log_sticky = GetBool(n, {"Log", "StickyBanner"}, f.log_sticky);
  f.dbc_path = GetScalar(n, {"Data", "DbcPath"}, f.dbc_path);
  f.motd = MotdFromYaml(n);
  if (f.motd.empty()) {
    f.motd =
        "Welcome to Firelands WoW!\n"
        "Enjoy your stay in the lands of Fire and Ice.";
  }
}

void AuthToYaml(YAML::Node &n, AuthFields const &f, std::string &err) {
  err.clear();
  int p = 0;
  int rp = 0;
  int rlp = 0;
  if (!ParseIntStrict(f.net_port, p)) {
    err = "Auth: Network.Port is not a valid integer.";
    return;
  }
  if (!ParseIntStrict(f.net_rest_port, rp)) {
    err = "Auth: Network.RestPort is not a valid integer.";
    return;
  }
  if (!ParseIntStrict(f.realm_link_port, rlp)) {
    err = "Auth: RealmLink.Port is not a valid integer.";
    return;
  }
  n["Network"]["BindAddress"] = f.net_bind;
  n["Network"]["Port"] = p;
  n["Network"]["RestPort"] = rp;
  n["Console"]["Enabled"] = f.console_enabled;
  n["Console"]["Tui"] = f.console_tui;
  n["RealmLink"]["Enabled"] = f.realm_link_enabled;
  n["RealmLink"]["BindAddress"] = f.realm_link_bind;
  n["RealmLink"]["Port"] = rlp;
  n["RealmLink"]["Token"] = f.realm_link_token;
  n["RealmList"]["HideRealmSuffixInAuthList"] = f.hide_realm_suffix;
  n["Database"]["User"] = f.db_user;
  n["Database"]["Password"] = f.db_password;
  n["Database"]["Auth"]["URI"] = f.db_auth_uri;
  n["Database"]["Characters"]["URI"] = f.db_chars_uri;
  n["Log"]["Level"] = f.log_level;
  n["Log"]["File"] = f.log_file;
  n["Log"]["StickyBanner"] = f.log_sticky;
}

void WorldToYaml(YAML::Node &n, WorldFields const &f, std::string &err) {
  err.clear();
  int p = 0;
  int ap = 0;
  int rid = 0;
  if (!ParseIntStrict(f.net_port, p)) {
    err = "World: Network.Port is not a valid integer.";
    return;
  }
  if (!ParseIntStrict(f.realm_auth_port, ap)) {
    err = "World: RealmLink.AuthPort is not a valid integer.";
    return;
  }
  if (!ParseIntStrict(f.realm_id, rid)) {
    err = "World: RealmLink.RealmId is not a valid integer.";
    return;
  }
  n["Network"]["BindAddress"] = f.net_bind;
  n["Network"]["Port"] = p;
  n["Console"]["Enabled"] = f.console_enabled;
  n["Console"]["Tui"] = f.console_tui;
  n["RealmLink"]["Enabled"] = f.realm_link_enabled;
  n["RealmLink"]["AuthHost"] = f.realm_auth_host;
  n["RealmLink"]["AuthPort"] = ap;
  n["RealmLink"]["RealmId"] = rid;
  n["RealmLink"]["Token"] = f.realm_token;
  n["World"]["RealmName"] = f.realm_name;
  n["Scripting"]["ScriptsDirectory"] = f.scripts_dir;
  n["Database"]["User"] = f.db_user;
  n["Database"]["Password"] = f.db_password;
  n["Database"]["Auth"]["URI"] = f.db_auth_uri;
  n["Database"]["Characters"]["URI"] = f.db_chars_uri;
  n["Database"]["World"]["URI"] = f.db_world_uri;
  n["Log"]["Level"] = f.log_level;
  n["Log"]["File"] = f.log_file;
  n["Log"]["StickyBanner"] = f.log_sticky;
  n["Data"]["DbcPath"] = f.dbc_path;
  MotdToYaml(n, f.motd);
}

bool WriteYamlFile(std::string const &path, YAML::Node const &root,
                   std::string &err) {
  err.clear();
  try {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
      err = "Could not open file for writing.";
      return false;
    }
    out << root;
    if (!out.flush()) {
      err = "Flush failed.";
      return false;
    }
    return true;
  } catch (std::exception const &e) {
    err = e.what();
    return false;
  }
}

Element ConfigTuiBanner() {
  static constexpr char const *const kBlockLogo[] = {
      "    ███████╗██╗██████╗ ███████╗██╗      █████╗ ███╗   ██╗██████╗ ███████╗",
      "    ██╔════╝██║██╔══██╗██╔════╝██║     ██╔══██╗████╗  ██║██╔══██╗██╔════╝",
      "    █████╗  ██║██████╔╝█████╗  ██║     ███████║██╔██╗ ██║██║  ██║███████╗",
      "    ██╔══╝  ██║██╔══██╗██╔══╝  ██║     ██╔══██║██║╚██╗██║██║  ██║╚════██║",
      "    ██║     ██║██║  ██║███████╗███████╗██║  ██║██║ ╚████║██████╔╝███████║",
      "    ╚═╝     ╚═╝╚═╝  ╚═╝╚══════╝╚══════╝╚═╝  ╚═╝╚═╝  ╚═══╝╚═════╝ ╚══════╝",
  };
  Color const rowColors[] = {
      Color::RGB(110, 38, 32),  Color::RGB(145, 48, 36), Color::RGB(185, 62, 40),
      Color::RGB(220, 85, 48),  Color::RGB(255, 118, 60), Color::RGB(255, 175, 130),
  };
  Elements logoRows;
  for (int i = 0; i < 6; ++i) {
    logoRows.push_back(text(kBlockLogo[i]) | bold | color(rowColors[i]));
  }

  Element const caption = center(hbox({
      text("Cataclysm WoW Emulator | ") | color(Color::RGB(232, 228, 220)),
      text("SERVER CONFIG EDITOR") | bold | color(Color::RGB(100, 205, 248)),
      text(" | YAML") | color(Color::RGB(232, 228, 220)),
  }));

  Element const rule =
      center(text(std::string(72, '-'))) | color(Color::RGB(95, 88, 82));

  return vbox({
             center(vbox(std::move(logoRows))),
             text(" ") | size(HEIGHT, EQUAL, 2),
             caption,
             text(" ") | size(HEIGHT, EQUAL, 1),
             rule,
         }) |
         bgcolor(Color::RGB(22, 20, 18)) |
         borderStyled(ROUNDED, Color::RGB(72, 64, 58));
}

} // namespace

int RunConfigEditorFtxui(int argc, char **argv) {
  std::optional<std::string> auth_path_override;
  std::optional<std::string> world_path_override;

  for (int i = 1; i < argc; ++i) {
    std::string const a = argv[i];
    if (a == "--auth" && i + 1 < argc) {
      auth_path_override = argv[++i];
    } else if (a == "--world" && i + 1 < argc) {
      world_path_override = argv[++i];
    } else if (a == "-h" || a == "--help") {
      continue;
    } else {
      std::cerr << "Unknown argument: " << a << "\n";
      return 2;
    }
  }

  if (!StdoutIsInteractiveTerminal()) {
    std::cerr << "firelands-config requires an interactive terminal (TTY).\n";
    return 2;
  }

  char const *argv0 = (argc > 0) ? argv[0] : nullptr;

  std::string auth_path =
      auth_path_override.value_or(ResolveConfigPath("authserver.yaml", argv0,
                                                    "FIRELANDS_AUTH_CONFIG")
                                      .value_or("authserver.yaml"));
  std::string world_path =
      world_path_override.value_or(ResolveConfigPath("worldserver.yaml", argv0,
                                                     "FIRELANDS_WORLD_CONFIG")
                                       .value_or("worldserver.yaml"));

  std::string load_err_auth;
  std::string load_err_world;
  YAML::Node auth_yaml = LoadYamlFileOrEmpty(auth_path, load_err_auth);
  YAML::Node world_yaml = LoadYamlFileOrEmpty(world_path, load_err_world);

  AuthFields auth{};
  WorldFields world{};
  YamlToAuth(auth_yaml, auth);
  YamlToWorld(world_yaml, world);

  IgnoreSigIntForTui const ignoreSigIntDuringTui;
  auto screen = ScreenInteractive::Fullscreen();
  Closure const requestExit = screen.ExitLoopClosure();

  std::string status_line =
      std::string("Auth: ") + auth_path + " | World: " + world_path;
  if (!load_err_auth.empty()) {
    status_line += std::string(" | Auth load: ") + load_err_auth;
  } else if (!fs::exists(auth_path)) {
    status_line += " | Auth: file missing (Save will create)";
  }
  if (!load_err_world.empty()) {
    status_line += std::string(" | World load: ") + load_err_world;
  } else if (!fs::exists(world_path)) {
    status_line += " | World: file missing (Save will create)";
  }

  int server_tab = 0;
  std::vector<std::string> server_toggle_entries = {
      " authserver.yaml ",
      " worldserver.yaml ",
  };
  auto server_toggle = Toggle(&server_toggle_entries, &server_tab);

  // --- Auth sections ---
  int auth_section = 0;
  std::vector<std::string> auth_section_labels = {
      "Network",
      "Console",
      "RealmLink",
      "Realm list",
      "Database",
      "Log",
  };
  auto auth_menu =
      Menu(&auth_section_labels, &auth_section, MenuOption::Vertical());

  auto in = [](std::string *s, char const *ph) { return Input(s, ph); };
  int constexpr kFieldGap = 2;

  Component auth_net = JoinVerticalSpaced(
      {
          NestedField(
              "Network.BindAddress", "Listen address (clients & REST)",
              "0.0.0.0 = all interfaces; 127.0.0.1 = local only.",
              in(&auth.net_bind, "value")),
          NestedField(
              "Network.Port", "WoW client login port",
              "TCP port for the authentication protocol (default 3724).",
              in(&auth.net_port, "integer")),
          NestedField(
              "Network.RestPort", "REST HTTP port",
              "Port for the REST API (e.g. tooling, web hooks).",
              in(&auth.net_rest_port, "integer")),
      },
      kFieldGap);

  Component auth_console = JoinVerticalSpaced(
      {
          NestedField(
              "Console.Enabled", "Console subsystem",
              "When off, no stdin / TUI on the auth process.",
              Checkbox("Enabled", &auth.console_enabled)),
          NestedField(
              "Console.Tui", "Fullscreen terminal UI",
              "FTXUI log screen; needs a real TTY (not pipes / Docker without -t).",
              Checkbox("Tui", &auth.console_tui)),
      },
      kFieldGap);

  Component auth_realm = JoinVerticalSpaced(
      {
          NestedField(
              "RealmLink.Enabled", "Realm link listener",
              "Private TCP so worldserver can mark realms online/offline instantly.",
              Checkbox("Enabled", &auth.realm_link_enabled)),
          NestedField(
              "RealmLink.BindAddress", "Realm link bind address",
              "Usually 127.0.0.1 if only local world processes connect.",
              in(&auth.realm_link_bind, "host")),
          NestedField(
              "RealmLink.Port", "Realm link TCP port",
              "Must match worldserver.yaml RealmLink.AuthPort.",
              in(&auth.realm_link_port, "integer")),
          NestedField(
              "RealmLink.Token", "Shared secret token",
              "Long random string; world RealmLink.Token must match exactly.",
              in(&auth.realm_link_token, "hex string")),
      },
      kFieldGap);

  Component auth_rlist = JoinVerticalSpaced(
      {
          NestedField(
              "RealmList.HideRealmSuffixInAuthList", "Hide realm suffix in auth list",
              "When true, AUTH_REALM_LIST sends an empty display name so 4.3.4 "
              "clients do not append \"-Realm\" in chat.",
              Checkbox("Hide suffix", &auth.hide_realm_suffix)),
      },
      kFieldGap);

  Component auth_db = JoinVerticalSpaced(
      {
          NestedField(
              "Database.User", "MariaDB user",
              "SQL user for JDBC connections below.",
              in(&auth.db_user, "username")),
          NestedField(
              "Database.Password", "MariaDB password",
              "Password for Database.User.",
              in(&auth.db_password, "password")),
          NestedField(
              "Database.Auth.URI", "Auth database JDBC URL",
              "jdbc:mariadb://host:3306/firelands_auth",
              in(&auth.db_auth_uri, "jdbc:mariadb://...")),
          NestedField(
              "Database.Characters.URI", "Characters DB JDBC URL",
              "Used for cross-realm character lookups from auth.",
              in(&auth.db_chars_uri, "jdbc:mariadb://...")),
      },
      kFieldGap);

  Component auth_log = JoinVerticalSpaced(
      {
          NestedField(
              "Log.Level", "Console log level",
              "trace, debug, info, warn, error, critical, off (case-insensitive).",
              in(&auth.log_level, "level")),
          NestedField(
              "Log.File", "Rotating log file path",
              "Relative to working directory unless absolute.",
              in(&auth.log_file, "path")),
          NestedField(
              "Log.StickyBanner", "Sticky log banner",
              "Fixed ANSI scroll region for logs; disable if it fights with TUI.",
              Checkbox("StickyBanner", &auth.log_sticky)),
      },
      kFieldGap);

  auto auth_tabs = Container::Tab(
      {
          auth_net,
          auth_console,
          auth_realm,
          auth_rlist,
          auth_db,
          auth_log,
      },
      &auth_section);

  Component auth_tabs_panel = Renderer(auth_tabs, [&] {
    return window(
        text(" Values ") | bold | color(Color::RGB(232, 226, 218)),
        vbox({
            text("") | size(HEIGHT, EQUAL, 1),
            auth_tabs->Render() | flex,
            text("") | size(HEIGHT, EQUAL, 1),
        }) | flex);
  });

  Component auth_menu_panel = Renderer(auth_menu, [&] {
    return window(
        text(" Section ") | bold | color(Color::RGB(232, 226, 218)),
        vbox({
            text("") | size(HEIGHT, EQUAL, 1),
            auth_menu->Render(),
            filler(),
            text("") | size(HEIGHT, EQUAL, 1),
        }));
  });

  Component auth_page = Container::Horizontal({
      auth_menu_panel,
      HSpace(3),
      auth_tabs_panel | flex,
  });

  // --- World sections ---
  int world_section = 0;
  std::vector<std::string> world_section_labels = {
      "Network",
      "Console",
      "RealmLink",
      "World",
      "Scripting",
      "Database",
      "Log",
      "Data",
      "Motd",
  };
  auto world_menu =
      Menu(&world_section_labels, &world_section, MenuOption::Vertical());

  Component world_net = JoinVerticalSpaced(
      {
          NestedField(
              "Network.BindAddress", "World listen address",
              "0.0.0.0 = all interfaces; bind before opening player connections.",
              in(&world.net_bind, "host")),
          NestedField(
              "Network.Port", "World server TCP port",
              "Game protocol port (not 3724; often 8085 or similar).",
              in(&world.net_port, "integer")),
      },
      kFieldGap);

  Component world_console = JoinVerticalSpaced(
      {
          NestedField(
              "Console.Enabled", "Interactive console",
              "stdin commands when TUI is off; auto-disabled if stdout is not a TTY.",
              Checkbox("Enabled", &world.console_enabled)),
          NestedField(
              "Console.Tui", "Fullscreen terminal UI",
              "FTXUI console; set Log.StickyBanner false if layout conflicts.",
              Checkbox("Tui", &world.console_tui)),
      },
      kFieldGap);

  Component world_realm = JoinVerticalSpaced(
      {
          NestedField(
              "RealmLink.Enabled", "Outbound link to auth",
              "Persistent TCP so auth marks this realm online / offline quickly.",
              Checkbox("Enabled", &world.realm_link_enabled)),
          NestedField(
              "RealmLink.AuthHost", "Auth realm-link host",
              "Hostname or IP of authserver RealmLink listener.",
              in(&world.realm_auth_host, "host")),
          NestedField(
              "RealmLink.AuthPort", "Auth realm-link port",
              "Must match authserver.yaml RealmLink.Port.",
              in(&world.realm_auth_port, "integer")),
          NestedField(
              "RealmLink.RealmId", "Realm ID",
              "Must match the id column in auth.realmlist for this realm.",
              in(&world.realm_id, "integer")),
          NestedField(
              "RealmLink.Token", "Shared secret token",
              "Same string as authserver.yaml RealmLink.Token.",
              in(&world.realm_token, "token")),
      },
      kFieldGap);

  Component world_world = JoinVerticalSpaced(
      {
          NestedField(
              "World.RealmName", "Realm display name",
              "Label for auth/realm link; chat shows the plain character name.",
              in(&world.realm_name, "name")),
      },
      kFieldGap);

  Component world_script = JoinVerticalSpaced(
      {
          NestedField(
              "Scripting.ScriptsDirectory", "Lua scripts root",
              "Directory for gameplay Lua (optional bootstrap.lua).",
              in(&world.scripts_dir, "path")),
      },
      kFieldGap);

  Component world_db = JoinVerticalSpaced(
      {
          NestedField(
              "Database.User", "MariaDB user",
              "SQL user for all three JDBC URLs below.",
              in(&world.db_user, "username")),
          NestedField(
              "Database.Password", "MariaDB password",
              "Password for Database.User.",
              in(&world.db_password, "password")),
          NestedField(
              "Database.Auth.URI", "Auth database JDBC URL",
              "jdbc:mariadb://host:3306/firelands_auth",
              in(&world.db_auth_uri, "jdbc:mariadb://...")),
          NestedField(
              "Database.Characters.URI", "Characters DB JDBC URL",
              "Character persistence and gameplay tables.",
              in(&world.db_chars_uri, "jdbc:mariadb://...")),
          NestedField(
              "Database.World.URI", "World database JDBC URL",
              "Spawns, templates, world state, DBC hotfixes in DB, etc.",
              in(&world.db_world_uri, "jdbc:mariadb://...")),
      },
      kFieldGap);

  Component world_log = JoinVerticalSpaced(
      {
          NestedField(
              "Log.Level", "Console log level",
              "trace … off; world defaults often use Debug during development.",
              in(&world.log_level, "level")),
          NestedField(
              "Log.File", "Rotating log file path",
              "Server log file on disk (relative unless absolute).",
              in(&world.log_file, "path")),
          NestedField(
              "Log.StickyBanner", "Sticky log banner",
              "ANSI scroll region; prefer off while using Console.Tui.",
              Checkbox("StickyBanner", &world.log_sticky)),
      },
      kFieldGap);

  Component world_data = JoinVerticalSpaced(
      {
          NestedField(
              "Data.DbcPath", "Client DBC directory",
              "4.3.4 client DBC folder (Spell.dbc, gt tables, Languages.dbc, …).",
              in(&world.dbc_path, "path")),
      },
      kFieldGap);

  InputOption motd_opt = InputOption::Default();
  motd_opt.content = &world.motd;
  motd_opt.placeholder = "One line per row (multiline MOTD)";
  Component world_motd = JoinVerticalSpaced(
      {
          NestedField(
              "Motd", "Message of the day (sequence)",
              "Each non-empty line becomes one YAML list entry sent to clients.",
              Input(motd_opt)),
      },
      kFieldGap);

  auto world_tabs = Container::Tab(
      {
          world_net,
          world_console,
          world_realm,
          world_world,
          world_script,
          world_db,
          world_log,
          world_data,
          world_motd,
      },
      &world_section);

  Component world_tabs_panel = Renderer(world_tabs, [&] {
    return window(
        text(" Values ") | bold | color(Color::RGB(232, 226, 218)),
        vbox({
            text("") | size(HEIGHT, EQUAL, 1),
            world_tabs->Render() | flex,
            text("") | size(HEIGHT, EQUAL, 1),
        }) | flex);
  });

  Component world_menu_panel = Renderer(world_menu, [&] {
    return window(
        text(" Section ") | bold | color(Color::RGB(232, 226, 218)),
        vbox({
            text("") | size(HEIGHT, EQUAL, 1),
            world_menu->Render(),
            filler(),
            text("") | size(HEIGHT, EQUAL, 1),
        }));
  });

  Component world_page = Container::Horizontal({
      world_menu_panel,
      HSpace(3),
      world_tabs_panel | flex,
  });

  auto server_pages =
      Container::Tab(
          {
              auth_page,
              world_page,
          },
          &server_tab);

  auto reload_clicked = [&] {
    std::string e1, e2;
    auth_yaml = LoadYamlFileOrEmpty(auth_path, e1);
    world_yaml = LoadYamlFileOrEmpty(world_path, e2);
    YamlToAuth(auth_yaml, auth);
    YamlToWorld(world_yaml, world);
    status_line = "Reloaded from disk.";
    if (!e1.empty()) {
      status_line += std::string(" Auth: ") + e1;
    }
    if (!e2.empty()) {
      status_line += std::string(" World: ") + e2;
    }
    screen.Post(Event::Custom);
  };

  auto save_clicked = [&] {
    std::string err;
    if (server_tab == 0) {
      AuthToYaml(auth_yaml, auth, err);
      if (!err.empty()) {
        status_line = err;
        screen.Post(Event::Custom);
        return;
      }
      if (!WriteYamlFile(auth_path, auth_yaml, err)) {
        status_line = std::string("Save auth failed: ") + err;
      } else {
        status_line = std::string("Saved auth: ") + auth_path;
      }
    } else {
      WorldToYaml(world_yaml, world, err);
      if (!err.empty()) {
        status_line = err;
        screen.Post(Event::Custom);
        return;
      }
      if (!WriteYamlFile(world_path, world_yaml, err)) {
        status_line = std::string("Save world failed: ") + err;
      } else {
        status_line = std::string("Saved world: ") + world_path;
      }
    }
    screen.Post(Event::Custom);
  };

  Component reload_btn = Button("Reload both", reload_clicked);
  Component save_btn = Button("Save active file", save_clicked);
  Component quit_btn = Button("Quit", requestExit);

  auto toolbar = Container::Horizontal({
      reload_btn,
      HSpace(3),
      save_btn,
      HSpace(3),
      quit_btn,
  });

  auto layout = Container::Vertical({
      server_toggle,
      VerticalGap(2),
      toolbar,
      VerticalGap(2),
      server_pages,
  });

  auto root = Renderer(layout, [&] {
    Element const title =
        text(server_tab == 0 ? "Editing: authserver.yaml"
                             : "Editing: worldserver.yaml") |
        bold | color(Color::RGB(240, 235, 228));

    Element const warn =
        paragraph(
            "Saving overwrites the YAML file. Comments are not preserved. "
            "RealmLink.Token must match between auth and world.") |
        color(Color::RGB(190, 175, 160));

    Element const status =
        hbox({
            text(" ") | bgcolor(kAccent),
            text(" " + status_line + " ") | flex,
        });

    return vbox({
               ConfigTuiBanner() | notflex,
               separator() | color(Color::RGB(110, 100, 92)),
               text("") | size(HEIGHT, EQUAL, 1),
               title | notflex,
               text("") | size(HEIGHT, EQUAL, 1),
               warn | notflex,
               text("") | size(HEIGHT, EQUAL, 1),
               separatorLight() | color(Color::RGB(90, 84, 78)),
               text("") | size(HEIGHT, EQUAL, 1),
               layout->Render() | flex,
               separator() | color(Color::RGB(110, 100, 92)),
               text("") | size(HEIGHT, EQUAL, 1),
               status | notflex,
               text("") | size(HEIGHT, EQUAL, 1),
               text("  Q  quit  ·  Tab / arrows navigate  ") | dim |
                   color(Color::RGB(170, 160, 150)),
           }) |
           bgcolor(kShellBg);
  });

  root = CatchEvent(root, [&](Event e) {
    if (!e.is_character() || e.character().size() != 1) {
      return false;
    }
    char const c = e.character()[0];
    if (c == 'q' || c == 'Q') {
      requestExit();
      return true;
    }
    return false;
  });

  screen.Loop(root);
  return 0;
}
