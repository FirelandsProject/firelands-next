#pragma once

namespace Firelands {

class IGmAppearanceCommandPort {
public:
  virtual ~IGmAppearanceCommandPort() = default;
  virtual void SetGmTagEnabled(bool on) { (void)on; }
  virtual void SetDndEnabled(bool on) { (void)on; }
  virtual void SetDevTagEnabled(bool on) { (void)on; }
  virtual void SetGmVisibleToPlayers(bool visible) { (void)visible; }
  virtual void SetGmFlyEnabled(bool on) { (void)on; }
  virtual void SetGmRunSpeed(float speed) { (void)speed; }
  virtual void OpenGmMailboxUi() {}
  virtual void OpenGmTicketUi() {}
};

} // namespace Firelands
