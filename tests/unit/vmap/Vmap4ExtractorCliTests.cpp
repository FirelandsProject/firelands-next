#include "Vmap4ExtractorCli.h"

#include "VMapMagic.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace FVV = Firelands::VMap::Vmap4Extractor;

namespace {

std::vector<char*> makeArgv(std::vector<std::string>& storage, std::initializer_list<const char*> parts) {
    storage.clear();
    for (char const* p : parts) {
        storage.emplace_back(p);
    }
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>("firelands-vmap4-extractor"));
    for (auto& s : storage) {
        argv.push_back(s.data());
    }
    return argv;
}

} // namespace

TEST(Vmap4ExtractorCli, HelpShortFlag) {
    std::vector<std::string> storage;
    std::vector<char*>       argv = makeArgv(storage, {"-h"});
    FVV::CliOptions          opts{};
    EXPECT_EQ(FVV::ParseCli(static_cast<int>(argv.size()), argv.data(), opts), FVV::CliParseResult::Help);
}

TEST(Vmap4ExtractorCli, HelpLongFlag) {
    std::vector<std::string> storage;
    std::vector<char*>       argv = makeArgv(storage, {"--help"});
    FVV::CliOptions          opts{};
    EXPECT_EQ(FVV::ParseCli(static_cast<int>(argv.size()), argv.data(), opts), FVV::CliParseResult::Help);
}

TEST(Vmap4ExtractorCli, HelpAfterPaths) {
    std::vector<std::string> storage;
    std::vector<char*> argv =
        makeArgv(storage, {"-d", "/wow", "-o", "/out", "--help"});
    FVV::CliOptions opts{};
    EXPECT_EQ(FVV::ParseCli(static_cast<int>(argv.size()), argv.data(), opts), FVV::CliParseResult::Help);
}

TEST(Vmap4ExtractorCli, OkMinimal) {
    std::vector<std::string> storage;
    std::vector<char*> argv = makeArgv(storage, {"-d", "/Applications/WoW", "-o", "/tmp/vmap-out"});
    FVV::CliOptions    opts{};
    ASSERT_EQ(FVV::ParseCli(static_cast<int>(argv.size()), argv.data(), opts), FVV::CliParseResult::Ok);
    EXPECT_EQ(opts.wowInstall.string(), "/Applications/WoW");
    EXPECT_EQ(opts.outputRoot.string(), "/tmp/vmap-out");
    EXPECT_EQ(opts.build, Firelands::VMap::kTargetBuild);
    EXPECT_FALSE(opts.preciseLargePayload);
    EXPECT_FALSE(opts.quiet);
}

TEST(Vmap4ExtractorCli, OkBuildQuietLarge) {
    std::vector<std::string> storage;
    std::vector<char*> argv =
        makeArgv(storage, {"-b", "15595", "-q", "-l", "-d", "/w", "-o", "/o"});
    FVV::CliOptions opts{};
    ASSERT_EQ(FVV::ParseCli(static_cast<int>(argv.size()), argv.data(), opts), FVV::CliParseResult::Ok);
    EXPECT_EQ(opts.build, 15595u);
    EXPECT_TRUE(opts.quiet);
    EXPECT_TRUE(opts.preciseLargePayload);
}

TEST(Vmap4ExtractorCli, ErrorMissingOut) {
    std::vector<std::string> storage;
    std::vector<char*>       argv = makeArgv(storage, {"-d", "/wow"});
    FVV::CliOptions          opts{};
    EXPECT_EQ(FVV::ParseCli(static_cast<int>(argv.size()), argv.data(), opts), FVV::CliParseResult::Error);
}

TEST(Vmap4ExtractorCli, ErrorUnknownFlag) {
    std::vector<std::string> storage;
    std::vector<char*> argv =
        makeArgv(storage, {"-d", "/w", "-o", "/o", "--not-a-flag"});
    FVV::CliOptions opts{};
    EXPECT_EQ(FVV::ParseCli(static_cast<int>(argv.size()), argv.data(), opts), FVV::CliParseResult::Error);
}

TEST(Vmap4ExtractorCli, ErrorBuildWithoutValue) {
    std::vector<std::string> storage;
    std::vector<char*>       argv = makeArgv(storage, {"-d", "/w", "-o", "/o", "-b"});
    FVV::CliOptions          opts{};
    EXPECT_EQ(FVV::ParseCli(static_cast<int>(argv.size()), argv.data(), opts), FVV::CliParseResult::Error);
}
