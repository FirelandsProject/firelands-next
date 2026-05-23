#include <application/services/WorldService.h>
#include <domain/world/Player.h>
#include <infrastructure/network/asio/AsioAwaitables.h>
#include <infrastructure/network/sessions/WorldSession.h>
#include <infrastructure/network/sessions/worldsession/WorldSessionObjectUpdate.h>
#include <boost/asio/redirect_error.hpp>
#include <shared/Config.h>
#include <shared/Logger.h>
#include <shared/game/MirrorTimerTypes.h>
#include <shared/network/WorldOpcodes.h>
#include <shared/network/WorldPacket.h>
#include <algorithm>
#include <cstdlib>

namespace Firelands {

namespace {

namespace ws_obj = WorldSessionObjectUpdate;

/// Cataclysm client expects periodic `SMSG_TIME_SYNC_REQ`; interval is driven by a
/// steady_timer (not by each `CMSG_TIME_SYNC_RESP`) to avoid flooding during map load.
uint32_t ClampedTimeSyncPeriodMs() {
  uint32_t const v = Config::Instance().GetNested<uint32_t>(
      {"Network", "TimeSyncPeriodMs"}, 300000u);
  if (v < 2000u)
    return 2000u;
  if (v > 3600000u)
    return 3600000u;
  return v;
}

} // namespace

void WorldSession::CancelPeriodicTimeSync() {
  _periodicTimeSyncRunning = false;
  _timeSyncPeriodicTimer.cancel();
}

void WorldSession::SchedulePeriodicTimeSync() {
  if (_periodicTimeSyncRunning.exchange(true))
    return;

  auto self = shared_from_this();
  Asio::SpawnDetached(_socket.get_executor(),
                      [self, this]() -> Asio::awaitable<void> {
                        co_await TimeSyncLoop();
                      });
}

Asio::awaitable<void> WorldSession::TimeSyncLoop() {
  try {
    while (_socket.is_open() && _periodicTimeSyncRunning) {
      _timeSyncPeriodicTimer.expires_after(
          std::chrono::milliseconds(ClampedTimeSyncPeriodMs()));
      boost::system::error_code ec;
      co_await _timeSyncPeriodicTimer.async_wait(
          boost::asio::redirect_error(Asio::use_awaitable, ec));
      if (ec == boost::asio::error::operation_aborted || !_periodicTimeSyncRunning)
        co_return;
      if (_playerGuid == 0)
        co_return;

      WorldPacket next(SMSG_TIME_SYNC_REQ);
      next.Append<uint32>(_timeSyncNextCounter++);
      SendPacket(next);
    }
  } catch (const boost::system::system_error &e) {
    if (e.code() != boost::asio::error::operation_aborted) {
      LOG_DEBUG("TimeSyncLoop ended: {}", e.what());
    }
  }
  _periodicTimeSyncRunning = false;
}

void WorldSession::SendStartMirrorTimerPacket(int32_t timerType, int32_t value,
                                            int32_t maxValue, int32_t scale,
                                            bool paused, int32_t spellId) {
  WorldPacket pkt(SMSG_START_MIRROR_TIMER);
  pkt.Append<int32>(timerType);
  pkt.Append<int32>(value);
  pkt.Append<int32>(maxValue);
  pkt.Append<int32>(scale);
  pkt.Append<uint8>(paused ? 1u : 0u);
  pkt.Append<int32>(spellId);
  SendPacket(pkt);
}

void WorldSession::SendStopMirrorTimerPacket(int32_t timerType) {
  WorldPacket pkt(SMSG_STOP_MIRROR_TIMER);
  pkt.Append<int32>(timerType);
  SendPacket(pkt);
}

void WorldSession::ResetBreathMirrorState() {
  if (_breathMirrorActive)
    SendStopMirrorTimerPacket(static_cast<int32_t>(MirrorTimerType::Breath));
  _breathMirrorActive = false;
  _breathRemainingMs = 0;
  _breathLastMonotonicTick.reset();
  _breathLastSentValueMs = -1;
}

void WorldSession::UpdateBreathFromSwimmingState(bool swimming) {
  if (_playerGuid == 0)
    return;

  if (!swimming) {
    ResetBreathMirrorState();
    return;
  }

  if (auto map = runtime().GetMap(_mapId)) {
    if (auto pl = map->TryGetPlayer(_playerGuid)) {
      if (pl->GetLiveHealth() == 0)
        return;
    }
  }

  auto const now = std::chrono::steady_clock::now();
  uint32_t dtMs = 0;
  if (_breathLastMonotonicTick.has_value()) {
    dtMs = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now - *_breathLastMonotonicTick)
            .count());
  }
  _breathLastMonotonicTick = now;
  if (dtMs > 5000u)
    dtMs = 5000u;

  if (!_breathMirrorActive) {
    _breathMirrorActive = true;
    _breathRemainingMs = kBreathMirrorMaxMs;
    SendStartMirrorTimerPacket(static_cast<int32_t>(MirrorTimerType::Breath),
                               _breathRemainingMs, kBreathMirrorMaxMs, -1, false,
                               0);
    _breathLastSentValueMs = _breathRemainingMs;
    return;
  }

  _breathRemainingMs -= static_cast<int32_t>(dtMs);

  while (_breathMirrorActive && _breathRemainingMs <= 0) {
    _breathRemainingMs += 1000;
    if (auto map = runtime().GetMap(_mapId)) {
      if (auto pl = map->TryGetPlayer(_playerGuid)) {
        uint8_t level = 1;
        if (auto ch = _charService->GetCharacterByGuid(_playerGuid))
          level = std::max<uint8_t>(1, ch->GetLevel());
        uint32 const maxHp = std::max<uint32>(1u, pl->GetLiveMaxHealth());
        uint32 const bonus =
            level > 1 ? static_cast<uint32>(rand() % static_cast<int>(level)) : 0u;
        int32_t const damage =
            std::max(1, static_cast<int32_t>(maxHp / 5u + bonus));
        pl->ApplyHealthDelta(-damage);
        WorldPacket hpUpdate;
        ws_obj::BuildPlayerHealthValuesUpdate(
            static_cast<uint16>(_mapId), _playerGuid, pl->GetLiveHealth(),
            pl->GetLiveMaxHealth(), hpUpdate);
        SendPacket(hpUpdate);
        map->BroadcastPacketToNearby(_playerGuid, hpUpdate, true);
        if (pl->GetLiveHealth() == 0) {
          ResetBreathMirrorState();
          return;
        }
      } else {
        break;
      }
    } else {
      break;
    }
  }

  int32_t const displayRemaining = std::max(0, _breathRemainingMs);
  int32_t const bucket = displayRemaining / 1000;
  int32_t const sentBucket = _breathLastSentValueMs / 1000;
  if (_breathLastSentValueMs < 0 || bucket != sentBucket) {
    SendStartMirrorTimerPacket(static_cast<int32_t>(MirrorTimerType::Breath),
                               displayRemaining, kBreathMirrorMaxMs, -1, false, 0);
    _breathLastSentValueMs = displayRemaining;
  }
}

} // namespace Firelands
