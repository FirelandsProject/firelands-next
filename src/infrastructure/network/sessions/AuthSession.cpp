#include <infrastructure/network/sessions/AuthSession.h>
#include <application/services/SRPService.h>
#include <shared/Logger.h>
#include <cstring>

namespace Firelands {

    AuthSession::AuthSession(tcp::socket socket, std::shared_ptr<AuthService> authService)
        : _socket(std::move(socket)), _authService(std::move(authService)) {
    }

    void AuthSession::Start() {
        DoRead();
    }

    void AuthSession::SendPacket(ByteBuffer& buffer) {
        auto self(shared_from_this());
        boost::asio::async_write(_socket, boost::asio::buffer(buffer.GetBuffer(), buffer.Size()),
            [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                if (ec) {
                    Close();
                }
            });
    }

    void AuthSession::Close() {
        boost::system::error_code ec;
        _socket.close(ec);
    }

    std::string AuthSession::GetIpAddress() const {
        try {
            return _socket.remote_endpoint().address().to_string();
        } catch (...) {
            return "unknown";
        }
    }

    void AuthSession::DoRead() {
        auto self(shared_from_this());
        _socket.async_read_some(boost::asio::buffer(_readBuffer, 1024),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    ByteBuffer buffer;
                    buffer.Append(_readBuffer, length);
                    HandlePacket(buffer);
                    DoRead();
                } else if (ec != boost::asio::error::operation_aborted) {
                    Close();
                }
            });
    }

    void AuthSession::HandlePacket(ByteBuffer& buffer) {
        if (buffer.Size() == 0) return;
        
        uint8 opcode = buffer.Read<uint8>();
        switch (opcode) {
            case AUTH_LOGON_CHALLENGE:
                HandleLogonChallenge(buffer);
                break;
            case AUTH_LOGON_PROOF:
                HandleLogonProof(buffer);
                break;
            default:
                LOG_WARN("Unknown opcode received: 0x{:02X}", opcode);
                Close();
                break;
        }
    }

    void AuthSession::HandleLogonChallenge(ByteBuffer& buffer) {
        AuthLogonChallenge_C challenge;
        challenge.Read(buffer);
        
        _username = challenge.username;
        LOG_INFO("Login challenge for user: {} ({})", _username, GetIpAddress());
        
        auto account = _authService->FindAccount(_username);
        
        AuthLogonChallenge_S response;
        response.opcode = AUTH_LOGON_CHALLENGE;
        
        if (!account) {
            response.result = AUTH_FAIL_UNKNOWN_ACCOUNT;
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
        
        ByteBuffer resBuffer;
        response.Write(resBuffer);
        SendPacket(resBuffer);
    }

    void AuthSession::HandleLogonProof(ByteBuffer& buffer) {
        AuthLogonProof_C proof;
        proof.Read(buffer);

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
            LOG_WARN("Login failed for {} - Wrong Password", _username);
        } else {
            response.result = AUTH_SUCCESS;
            auto M2 = SRPService::CalculateM2(A, M1, K);
            std::memcpy(response.M2, M2.data(), 20);
            response.account_flags = 0;
            response.survey_id = 0;
            response.login_flags = 0;
            LOG_INFO("Login successful for {}", _username);
        }

        ByteBuffer resBuffer;
        response.Write(resBuffer);
        SendPacket(resBuffer);
    }

    void AuthSession::DoWrite() {
        // Not used currently as we use async_write directly
    }

} // namespace Firelands
