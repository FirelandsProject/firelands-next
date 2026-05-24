#include <gtest/gtest.h>
#include <shared/Logger.h>
#include <shared/tui/FtxuiLogSink.h>
#include <shared/tui/FtxuiLogSpdlog.h>

using namespace Firelands;

class FtxuiLoggerBindingTest : public ::testing::Test {
protected:
  void TearDown() override {
    if (Logger::IsInitialized()) {
      Logger::Shutdown();
    }
  }
};

TEST_F(FtxuiLoggerBindingTest, LogInfoAppearsInFtxuiSink) {
  Logger::Init(LoggerBuilder().WithConsole(true).WithFile(false).Build());
  auto const sink = std::make_shared<FtxuiLogSink>(256);
  BindFirelandsLoggerToFtxuiSink(sink);

  LOG_INFO("firelands tui log line");
  Logger::Get().Flush();

  EXPECT_GE(sink->LineCount(), 1u);
  auto const lines = sink->CopyRecentLines(8);
  ASSERT_FALSE(lines.empty());
  EXPECT_NE(lines.back().find("firelands tui log line"), std::string::npos);
  EXPECT_NE(lines.back().find("💬"), std::string::npos);
}
