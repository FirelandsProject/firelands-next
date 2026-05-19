#include <domain/models/NpcText.h>
#include <gtest/gtest.h>

namespace Firelands {

TEST(NpcTextTests, MakeFallback_SetsFirstOptionProbabilityAndGreeting) {
  auto const text = NpcText::MakeFallback(3466, "Hello $N");
  EXPECT_EQ(text.id, 3466u);
  EXPECT_FLOAT_EQ(text.options[0].probability, 1.f);
  EXPECT_EQ(text.options[0].text0, "Hello $N");
  EXPECT_EQ(text.options[0].text1, "Hello $N");
  EXPECT_FLOAT_EQ(text.options[1].probability, 0.f);
  EXPECT_TRUE(text.options[1].text0.empty());
}

} // namespace Firelands
