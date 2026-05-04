#include <gtest/gtest.h>
#include <shared/dbc/FactionTemplateDbc.h>

using namespace Firelands;

TEST(FactionTemplateDbcTests, LoadMissingFile_ReturnsFalse) {
  FactionTemplateDbc dbc;
  EXPECT_FALSE(dbc.Load("/nonexistent/dir/FactionTemplate.dbc"));
  EXPECT_FALSE(dbc.IsLoaded());
  EXPECT_FALSE(dbc.HasEntry(14));
}
