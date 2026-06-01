#include <application/services/SRPService.h>
#include <boost/asio/redirect_error.hpp>
#include <cstring>
#include <infrastructure/network/asio/AsioAwaitables.h>
#include <infrastructure/network/sessions/AuthSession.h>
#include <shared/Config.h>
#include <domain/repositories/IRbacRepository.h>
#include <shared/game/RbacBuiltinRoles.h>
#include <shared/Logger.h>

#include <mutex>

namespace Firelands {

AuthSession::AuthSession(tcp::socket socket,
                         std::shared_ptr<AuthService> authService,
                         std::shared_ptr<RealmListService> realmService,
                         std::shared_ptr<IRbacRepository> rbacRepo)
    : _socket(std::move(socket)), _authService(std::move(authService)),
      _realmService(std::move(realmService)), _rbacRepo(std::move(rbacRepo)),
      _writeWakeTimer(_socket.get_executor()) {}

void AuthSession::Start() {
  auto self = shared_from_this();
  auto const executor = _socket.get_executor();
  Asio::SpawnDetached(executor, [self, this]() -> Asio::awaitable<void> {
    co_await ReadLoop();
  });
  Asio::SpawnDetached(executor, [self, this]() -> Asio::awaitable<void> {
    co_await WriteLoop();
  });
  }

void AuthSession::SendPacket(AuthPacket &packet) {
  // Auth packets are simple: opcode + payload
  // No extra header size like World packets, except for specific responses
  // that are handled by the packet struct itself.
  SendPacket(static_cast<ByteBuffer &>(packet));
  }

void AuthSession::SendPacket(ByteBuffer &buffer) {
  auto shared_buffer = std::make_shared<std::vector<uint8>>(
      buffer.GetBuffer(), buffer.GetBuffer() + buffer.Size());
  QueueOutgoing(std::move(shared_buffer));
  }

void AuthSession::QueueOutgoing(std::shared_ptr<std::vector<uint8>> buffer) {
  {
    std::lock_guard<std::mutex> lock(_writeMutex);
    _writeQueue.push_back(std::move(buffer));
  }
  _writeWakeTimer.cancel();
  }

void AuthSession::Close() {
  _writeWakeTimer.cancel();
  if (_socket.is_open()) {
    _socket.close();
  }
  }

std::string AuthSession::GetIpAddress() const {
  try {
    return _socket.remote_endpoint().address().to_string();
  } catch (...) {
    return "unknown";
  }
  }

Asio::awaitable<void> AuthSession::ReadLoop() {
  const std::span<uint8_t> readSpan(_readBuffer, sizeof(_readBuffer));

  try {
    while (_socket.is_open()) {
      std::size_t const length = co_await Asio::AsyncReadSome(_socket, readSpan);
      if (length == 0)
        continue;

      ByteBuffer buffer;
      buffer.Append(std::span<const uint8>(_readBuffer, length));
      HandlePacket(buffer);
  }
  } catch (const boost::system::system_error &e) {
    if (e.code() != boost::asio::error::operation_aborted &&
        e.code() != boost::asio::error::eof) {
      LOG_DEBUG("AuthSession read ended: {}", e.what());
  }
    Close();
  }
  }

Asio::awaitable<void> AuthSession::WriteLoop() {
  try {
    for (;;) {
      std::shared_ptr<std::vector<uint8>> buffer;

  {
        std::unique_lock<std::mutex> lock(_writeMutex);
        while (_writeQueue.empty()) {
          if (!_socket.is_open())
            co_return;

          lock.unlock();
          boost::system::error_code ec;
          _writeWakeTimer.expires_at(boost::asio::steady_timer::time_point::max());
          co_await _writeWakeTimer.async_wait(
              boost::asio::redirect_error(Asio::use_awaitable, ec));
          lock.lock();
  }

        buffer = std::move(_writeQueue.front());
        _writeQueue.pop_front();
  }

      co_await Asio::AsyncWrite(
          _socket, std::span<const uint8>(buffer->data(), buffer->size()));
  }
  } catch (const boost::system::system_error &e) {
    if (e.code() != boost::asio::error::operation_aborted) {
      LOG_DEBUG("AuthSession write ended: {}", e.what());
  }
    Close();
  }
  }

void AuthSession::HandlePacket(ByteBuffer &buffer) {
  if (buffer.Size() == 0)
    return;

  uint8 opcode = buffer.Read<uint8>();
  AuthPacket packet(opcode, buffer.Size());
  if (buffer.Size() > 0) {
    packet.Append(buffer.GetBuffer() + 1, buffer.Size() - 1);
  }

  ProcessPacket(packet);
  }

void AuthSession::ProcessPacket(AuthPacket &packet) {
  uint8 opcode = packet.GetOpcode();
  LOG_DEBUG("AuthSession received packet: {}, size: {}", packet.GetOpcodeName(),
             packet.Size());

  switch (opcode) {
  case AUTH_LOGON_CHALLENGE:
    HandleLogonChallenge(packet);
    break;
  case AUTH_LOGON_PROOF:
    HandleLogonProof(packet);
    break;
  case AUTH_REALM_LIST:
    HandleRealmList(packet);
    break;
  default:
    LOG_WARN("Unknown opcode received: 0x{:02X}", opcode);
    Close();
    break;
  }
  }

void AuthSession::HandleLogonChallenge(AuthPacket &packet) {
  AuthLogonChallenge_C challenge;
  challenge.Read(packet);

  _username = challenge.username;
  _accountRolePermissionMask = 0;
  LOG_DEBUG("Login challenge for user: {} ({})", _username, GetIpAddress());

  auto account = _authService->FindAccount(_username);

  AuthLogonChallenge_S response;
  response.opcode = AUTH_LOGON_CHALLENGE;

  if (!account) {
    response.result = AUTH_FAIL_UNKNOWN_ACCOUNT;
  } else if (account->locked) {
    response.result = AUTH_FAIL_BANNED;
    LOG_WARN("Login challenge rejected (locked): {} ({})", _username, GetIpAddress());
  } else {
    response.result = AUTH_SUCCESS;

    // Save salt for later
    _salt = account->salt;

    // Generate SRP-6a b and B
    _v = std::make_unique<BigInt>(account->verifier);
    _b = std::make_unique<BigInt>(SRPService::GeneratePrivateB());
    _B = std::make_unique<BigInt>(SRPService::CalculateB(*_v, *_b));

    std::vector<uint8> B_bytes = _B->ToBinary(32);
    std::reverse(B_bytes.begin(), B_bytes.end());
    response.B = B_bytes;

    std::vector<uint8> N_bytes = SRP::N;
    std::reverse(N_bytes.begin(), N_bytes.end());
    response.N = N_bytes;

    response.salt = _salt;

    response.gLen = 1;
    response.g = SRP::g;
    response.NLen = 32;
    response.unk3.assign(16, 0);
    response.securityFlags = 0;
  }

  AuthPacket res(AUTH_LOGON_CHALLENGE);
  response.Write(res);
  SendPacket(res);
  }

void AuthSession::HandleLogonProof(AuthPacket &packet) {
  AuthLogonProof_C proof;
  proof.Read(packet);

  if (!_v || !_b || !_B) {
    Close();
    return;
  }

  std::vector<uint8> A_bytes(proof.A, proof.A + 32);
  std::reverse(A_bytes.begin(), A_bytes.end());
  BigInt A(A_bytes);

  auto K = SRPService::CalculateSessionKey(A, *_B, *_v, *_b);
  auto M1 = SRPService::CalculateM1(_username, A, *_B, _salt, K);

  AuthLogonProof_S response;
  response.opcode = AUTH_LOGON_PROOF;

  if (std::memcmp(M1.data(), proof.M1, 20) != 0) {
    response.result = AUTH_FAIL_WRONG_PASSWORD;
    LOG_WARN("Auth failed: IP={} User='{}' Reason=Wrong Password", GetIpAddress(), _username);
  _accountRolePermissionMask = 0;
  } else {
    response.result = AUTH_SUCCESS;
    auto M2 = SRPService::CalculateM2(A, M1, K);
    std::memcpy(response.M2, M2.data(), 20);
    response.account_flags = 0;
    response.survey_id = 0;
    response.login_flags = 0;

    // Persist the session key so the World Server can validate it
  auto account = _authService->FindAccount(_username);
    uint32 accountId = account ? account->id : 0;
    if (account) {
      _authService->CreateSession(account->id, K);
      if (_rbacRepo)
        _accountRolePermissionMask = static_cast<PermissionMask>(
            _rbacRepo->UnionPermissionMaskForAccount(account->id));
      else
        _accountRolePermissionMask = 0;
    } else {
      _accountRolePermissionMask = 0;
    }
    LOG_INFO("Auth success: IP={} User='{}' AccountId={} StaffTier={}",
             GetIpAddress(), _username, accountId,
             EffectiveStaffTierRank(_accountRolePermissionMask));
  }

  AuthPacket res(AUTH_LOGON_PROOF);
  response.Write(res);
  SendPacket(res);
  }

void AuthSession::HandleRealmList(AuthPacket & /*packet*/) {
  static std::once_flag realmLinkConfigHint;

  AuthRealmList_S response;
  response.opcode = AUTH_REALM_LIST;

  std::vector<Realm> realms;
  if (_realmService) {
    realms = _realmService->GetRealmList();
  }

  std::vector<Realm const *> visibleRealms;
  visibleRealms.reserve(realms.size());
  for (Realm const &r : realms) {
    if (MeetsRealmSecurityRequirement(_accountRolePermissionMask,
                                      r.GetAllowedSecurityLevel()))
      visibleRealms.push_back(&r);
  }

  bool const liveMerge =
      _realmService && _realmService->UsesLiveRealmState();
  LOG_DEBUG("Realm list for {}: {} realm(s) ({} after access filter), "
            "realmLinkMerge={}",
            _username, realms.size(), visibleRealms.size(),
            liveMerge ? "on" : "off");
  if (!liveMerge) {
    std::call_once(realmLinkConfigHint, [] {
      LOG_WARN(
          "Realm-link merge is OFF (empty RealmLink.Token or Port in "
          "authserver.yaml). The list comes only from the DB — killing the "
          "world will not change realm status until you configure RealmLink "
          "and set RealmLink.Enabled on the world.");
  });
  }

  ByteBuffer payloadBuffer;

  payloadBuffer.Append<uint32>(0); // unknown
  payloadBuffer.Append<uint16>(static_cast<uint16>(visibleRealms.size()));

  for (Realm const *rp : visibleRealms) {
    Realm const &realm = *rp;
  // Wire order: Type, Lock (0/1), RealmFlags, name, address, pop, ...
    payloadBuffer.Append<uint8>(realm.GetIcon());
    // Realms below the account tier are omitted; visible realms are joinable.
    payloadBuffer.Append<uint8>(0);
    payloadBuffer.Append<uint8>(realm.GetRealmListFlags());
    // Name (ByteBuffer::Append(std::string) adds null terminator). The 4.3.4
    // client uses this label for "Player-Realm" in chat; optional auth config
    // can send "" so only the character name appears (DB `realmlist.name` unchanged).
    std::string const realmWireName =
        Config::Instance().GetNestedBool(
            {"RealmList", "HideRealmSuffixInAuthList"}, false)
            ? std::string()
            : realm.GetName();
    payloadBuffer.Append(realmWireName);

    // Address:port
    std::string address_port =
        realm.GetAddress() + ":" + std::to_string(realm.GetPort());
    payloadBuffer.Append(address_port);

    payloadBuffer.Append<float>(realm.GetPopulation());
    payloadBuffer.Append<uint8>(0); // characters count
    payloadBuffer.Append<uint8>(realm.GetTimezone());
    payloadBuffer.Append<uint8>(static_cast<uint8>(realm.GetId())); // Realm ID
  }

  payloadBuffer.Append<uint16>(0x0010); // unknown2

  response.payload =
      std::vector<uint8>(payloadBuffer.GetBuffer(),
                         payloadBuffer.GetBuffer() + payloadBuffer.Size());

  AuthPacket res(AUTH_REALM_LIST);
  response.Write(res);
  SendPacket(res);
  }

} // namespace Firelands
