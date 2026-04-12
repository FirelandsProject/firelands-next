#ifndef FIRELANDS_SHARED_CRYPTO_H
#define FIRELANDS_SHARED_CRYPTO_H

#include <string>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <openssl/sha.h>
#include <openssl/evp.h>

namespace Firelands {
namespace Crypto {

    using SHA1Hash = std::vector<uint8_t>;

    inline std::string ToUpper(std::string str) {
        std::transform(str.begin(), str.end(), str.begin(), ::toupper);
        return str;
    }

    inline SHA1Hash CalculateSHA1(const std::string& input) {
        SHA1Hash hash(SHA_DIGEST_LENGTH);
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        EVP_DigestInit_ex(ctx, EVP_sha1(), nullptr);
        EVP_DigestUpdate(ctx, input.c_str(), input.length());
        unsigned int len = 0;
        EVP_DigestFinal_ex(ctx, hash.data(), &len);
        EVP_MD_CTX_free(ctx);
        return hash;
    }

    inline SHA1Hash CalculateSHA1(const std::vector<uint8_t>& input) {
        SHA1Hash hash(SHA_DIGEST_LENGTH);
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        EVP_DigestInit_ex(ctx, EVP_sha1(), nullptr);
        EVP_DigestUpdate(ctx, input.data(), input.size());
        unsigned int len = 0;
        EVP_DigestFinal_ex(ctx, hash.data(), &len);
        EVP_MD_CTX_free(ctx);
        return hash;
    }

    inline std::string ToHexString(const SHA1Hash& hash) {
        std::stringstream ss;
        ss << std::hex << std::setfill('0') << std::uppercase;
        for (auto byte : hash) {
            ss << std::setw(2) << static_cast<int>(byte);
        }
        return ss.str();
    }

} // namespace Crypto
} // namespace Firelands

#endif // FIRELANDS_SHARED_CRYPTO_H
