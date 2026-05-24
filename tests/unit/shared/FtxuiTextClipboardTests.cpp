#include <shared/tui/FtxuiTextClipboard.h>

#include <ftxui/component/event.hpp>

#include <gtest/gtest.h>

namespace Firelands {
namespace {

TEST(FtxuiTextClipboardTests, InsertTextAtCursor_SingleLineFlattensNewlines) {
  std::string text = "ab";
  int cursor = 1;
  InsertTextAtCursor(text, cursor, "X\nY", {.multiline = false});
  EXPECT_EQ(text, "aX Yb");
  EXPECT_EQ(cursor, 4);
}

TEST(FtxuiTextClipboardTests, InsertTextAtCursor_MultilineKeepsNewlines) {
  std::string text;
  int cursor = 0;
  InsertTextAtCursor(text, cursor, "line1\nline2", {.multiline = true});
  EXPECT_EQ(text, "line1\nline2");
  EXPECT_EQ(cursor, 11);
}

TEST(FtxuiTextClipboardTests, TryHandleTextClipboardEvent_CtrlA_MovesToStart) {
  std::string text = "hello";
  int cursor = 5;
  EXPECT_TRUE(TryHandleTextClipboardEvent(ftxui::Event::Character('\1'), text, cursor,
                                          {}));
  EXPECT_EQ(cursor, 0);
  EXPECT_EQ(text, "hello");
}

TEST(FtxuiTextClipboardTests, TryHandleTextClipboardEvent_TerminalPaste) {
  std::string text = "hi";
  int cursor = 2;
  EXPECT_TRUE(TryHandleTextClipboardEvent(ftxui::Event::Character(" there"), text,
                                          cursor, {}));
  EXPECT_EQ(text, "hi there");
  EXPECT_EQ(cursor, 8);
}

} // namespace
} // namespace Firelands
