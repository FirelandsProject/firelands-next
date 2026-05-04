#include <gtest/gtest.h>
#include <shared/dbc/ChrRacesDbc.h>

using namespace Firelands;

TEST(ChrRacesDbcTests, LoadMissingFile_ReturnsFalse) {
  ChrRacesDbc dbc;
  EXPECT_FALSE(dbc.Load("/nonexistent/ChrRaces.dbc"));
  EXPECT_FALSE(dbc.IsLoaded());
  EXPECT_FALSE(dbc.FactionTemplateIdForRace(1).has_value());
}
