#include <gtest/gtest.h>
#include <infrastructure/network/sessions/worldsession/GmTicketGossipUi.h>

namespace Firelands::gm_ticket_ui {
namespace {

TEST(GmTicketGossipUi, ReservedIds_DoNotOverlapDefaultGossip) {
  EXPECT_FALSE(IsReservedMenu(0));
  EXPECT_FALSE(IsReservedMenu(2782));
  EXPECT_TRUE(IsReservedMenu(kMenuMain));
  EXPECT_TRUE(IsReservedText(kNpcTextDetail));
  EXPECT_FALSE(IsReservedText(3466));
}

TEST(GmTicketGossipUi, TruncateForGossipOption_AppendsEllipsis) {
  std::string const longText(120, 'x');
  auto const out = TruncateForGossipOption(longText, 20);
  EXPECT_EQ(out.size(), 20u);
  EXPECT_EQ(out.substr(out.size() - 3), "...");
}

} // namespace
} // namespace Firelands::gm_ticket_ui
