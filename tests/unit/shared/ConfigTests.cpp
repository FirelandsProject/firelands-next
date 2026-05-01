#include <gtest/gtest.h>
#include <shared/Config.h>
#include <string>

using namespace Firelands;

TEST(ConfigTest, LogLevelConversion) {
    YAML::Node node = YAML::Load("trace");
    EXPECT_EQ(node.as<LogLevel>(), LogLevel::Trace);

    node = YAML::Load("DEBUG");
    EXPECT_EQ(node.as<LogLevel>(), LogLevel::Debug);

    node = YAML::Load("Info");
    EXPECT_EQ(node.as<LogLevel>(), LogLevel::Info);

    node = YAML::Load("WARN");
    EXPECT_EQ(node.as<LogLevel>(), LogLevel::Warn);

    node = YAML::Load("error");
    EXPECT_EQ(node.as<LogLevel>(), LogLevel::Error);

    node = YAML::Load("CRITICAL");
    EXPECT_EQ(node.as<LogLevel>(), LogLevel::Critical);

    node = YAML::Load("off");
    EXPECT_EQ(node.as<LogLevel>(), LogLevel::Off);
}

TEST(ConfigTest, NestedAccess) {
    YAML::Node root = YAML::Load("Logging:\n  Console: debug\n  File: error");
    
    // Test basic yaml-cpp access
    EXPECT_EQ(root["Logging"]["Console"].as<LogLevel>(), LogLevel::Debug);
    EXPECT_EQ(root["Logging"]["File"].as<LogLevel>(), LogLevel::Error);
}

TEST(ConfigTest, ReadsRealmLinkFromProjectAuthYaml) {
  Config &c = Config::Instance();
  std::string const path =
      std::string(FIRELANDS_TEST_DATA_DIR) + "/authserver.yaml";
  ASSERT_TRUE(c.Load(path));
  ASSERT_TRUE(c.HasNestedKey({"RealmLink", "Token"}));
  EXPECT_FALSE(c.GetNestedScalarString({"RealmLink", "Token"}, "").empty());
  EXPECT_EQ(c.GetNested<int>({"RealmLink", "Port"}, 0), 3725);
}

TEST(ConfigTest, GetNestedBoolScalarsAndYamlBool) {
  Config &c = Config::Instance();
  ASSERT_TRUE(c.Load(std::string(FIRELANDS_TEST_DATA_DIR) +
                     "/tests/data/sticky_banner_config.yaml"));
  EXPECT_TRUE(c.GetNestedBool({"Log", "StickyBanner"}, false));
  EXPECT_TRUE(c.GetNestedBool({"Alt", "FromYes"}, false));
  EXPECT_FALSE(c.GetNestedBool({"Alt", "FromOff"}, true));
}
