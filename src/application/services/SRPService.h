#ifndef FIRELANDS_APPLICATION_SRP_SERVICE_H
#define FIRELANDS_APPLICATION_SRP_SERVICE_H

#include <shared/SRPConstants.h>
#include <shared/BigInt.h>
#include <shared/Crypto.h>
#include <shared/network/ByteBuffer.h>
#include <random>
#include <array>
#include <vector>
#include <string>
#include <algorithm>

namespace Firelands {

    struct SRPData {
        std::vector<uint8_t> salt;
        std::vector<uint8_t> verifier;
    };

    class SRPService {
    public:
        static SRPData GenerateVerifier(const std::string& username, const std::string& password) {
            std::vector<uint8_t> salt(32);
            std::random_device rd;
            std::generate(salt.begin(), salt.end(), [&]() { return static_cast<uint8_t>(rd() % 256); });

            std::string identity = Crypto::ToUpper(username);
            std::string pass = Crypto::ToUpper(password);
            
            auto h1 = Crypto::CalculateSHA1(identity + ":" + pass);
            
            // x = SHA1(s | SHA1(I:P))
            std::vector<uint8_t> saltAndH1 = salt;
            saltAndH1.insert(saltAndH1.end(), h1.begin(), h1.end());
            auto x_bytes = Crypto::CalculateSHA1(saltAndH1);
            std::reverse(x_bytes.begin(), x_bytes.end()); // To Little Endian for BigInt? No, BigInt expects BE.

            BigInt bn_g(SRP::g);
            BigInt bn_x(x_bytes);
            BigInt bn_N(SRP::N);

            BigInt bn_v = BigInt::ModExp(bn_g, bn_x, bn_N);

            return { salt, bn_v.ToBinary(32) };
        }

        static BigInt GeneratePrivateB() {
            std::vector<uint8_t> b_bytes(19);
            std::random_device rd;
            std::generate(b_bytes.begin(), b_bytes.end(), [&]() { return static_cast<uint8_t>(rd() % 256); });
            return BigInt(b_bytes);
        }

        static BigInt CalculateB(const BigInt& v, const BigInt& b) {
            BigInt bn_g(SRP::g);
            BigInt bn_N(SRP::N);
            BigInt bn_k(SRP::k);

            // B = (kv + g^b) % N
            BigInt gb = BigInt::ModExp(bn_g, b, bn_N);
            BigInt kv = BigInt::ModMul(bn_k, v, bn_N);
            
            return BigInt::ModAdd(kv, gb, bn_N);
        }

        static std::vector<uint8_t> CalculateSessionKey(const BigInt& A, const BigInt& B, const BigInt& v, const BigInt& b) {
            BigInt bn_N(SRP::N);

            // u = H(A_LE | B_LE)
            std::vector<uint8_t> A_le = A.ToBinary(32);
            std::vector<uint8_t> B_le = B.ToBinary(32);
            std::reverse(A_le.begin(), A_le.end());
            std::reverse(B_le.begin(), B_le.end());

            std::vector<uint8_t> AB = A_le;
            AB.insert(AB.end(), B_le.begin(), B_le.end());
            auto u_bytes = Crypto::CalculateSHA1(AB);
            std::reverse(u_bytes.begin(), u_bytes.end()); // To BE for BigInt
            BigInt u(u_bytes);

            // S = (A * v^u)^b % N
            BigInt vu = BigInt::ModExp(v, u, bn_N);
            BigInt Avu = BigInt::ModMul(A, vu, bn_N);
            BigInt S = BigInt::ModExp(Avu, b, bn_N);

            auto s_bytes = S.ToBinary(32);
            std::reverse(s_bytes.begin(), s_bytes.end());
            
            // WoW Interleaved Session Key derivation
            std::vector<uint8_t> buf0(16), buf1(16);
            for (size_t i = 0; i < 16; ++i) {
                buf0[i] = s_bytes[i * 2];
                buf1[i] = s_bytes[i * 2 + 1];
            }

            size_t p = 0;
            while (p < 32 && !s_bytes[p]) ++p;
            if (p & 1) ++p; // skip one extra byte if p is odd
            p /= 2; // offset into buffers

            std::vector<uint8_t> to_hash0(buf0.begin() + p, buf0.end());
            std::vector<uint8_t> to_hash1(buf1.begin() + p, buf1.end());

            auto h1 = Crypto::CalculateSHA1(to_hash0);
            auto h2 = Crypto::CalculateSHA1(to_hash1);

            std::vector<uint8_t> K(40);
            for (size_t i = 0; i < 20; ++i) {
                K[i * 2] = h1[i];
                K[i * 2 + 1] = h2[i];
            }

            return K;
        }

        static std::vector<uint8_t> CalculateM1(const std::string& username, const BigInt& A, const BigInt& B, const std::vector<uint8_t>& salt, const std::vector<uint8_t>& K) {
            // M1 = H(H(N_LE) ^ H(g_LE), H(I), s_LE, A_LE, B_LE, K)
            std::vector<uint8_t> n_le = SRP::N;
            std::reverse(n_le.begin(), n_le.end());
            auto hN = Crypto::CalculateSHA1(n_le);
            auto hg = Crypto::CalculateSHA1(std::vector<uint8_t>{SRP::g});
            
            std::vector<uint8_t> hNg(20);
            for (size_t i = 0; i < 20; ++i) hNg[i] = hN[i] ^ hg[i];

            auto hI = Crypto::CalculateSHA1(Crypto::ToUpper(username));

            ByteBuffer buffer;
            buffer.Append(hNg.data(), hNg.size());
            buffer.Append(hI.data(), hI.size());
            
            // salt (use as is, no reversal needed for seed)
            buffer.Append(salt.data(), salt.size());
            
            // A_LE
            auto a_le = A.ToBinary(32);
            std::reverse(a_le.begin(), a_le.end());
            buffer.Append(a_le.data(), a_le.size());
            
            // B_LE
            auto b_le = B.ToBinary(32);
            std::reverse(b_le.begin(), b_le.end());
            buffer.Append(b_le.data(), b_le.size());
            
            // K (already interleaved 40 bytes)
            buffer.Append(K.data(), K.size());

            std::vector<uint8_t> data(buffer.GetBuffer(), buffer.GetBuffer() + buffer.Size());
            return Crypto::CalculateSHA1(data);
        }

        static std::vector<uint8_t> CalculateM2(const BigInt& A, const std::vector<uint8_t>& M1, const std::vector<uint8_t>& K) {
            // M2 = H(A_LE, M1, K)
            ByteBuffer buffer;
            auto a_le = A.ToBinary(32);
            std::reverse(a_le.begin(), a_le.end());
            buffer.Append(a_le.data(), a_le.size());
            buffer.Append(M1.data(), M1.size());
            buffer.Append(K.data(), K.size());

            std::vector<uint8_t> data(buffer.GetBuffer(), buffer.GetBuffer() + buffer.Size());
            return Crypto::CalculateSHA1(data);
        }
    };

} // namespace Firelands

#endif // FIRELANDS_APPLICATION_SRP_SERVICE_H
