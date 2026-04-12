#ifndef FIRELANDS_SHARED_CRYPTO_H
#define FIRELANDS_SHARED_CRYPTO_H

#include <string>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <openssl/sha.h>

namespace Firelands {
namespace Crypto {

    using SHA1Hash = std::vector<uint8_t>;

    inline std::string ToUpper(std::string str) {
        std::transform(str.begin(), str.end(), str.begin(), ::toupper);
        return str;
    }

    inline SHA1Hash CalculateSHA1(const std::string& input) {
        SHA1Hash hash(SHA_DIGEST_LENGTH);
        SHA1(reinterpret_cast<const uint8_t*>(input.c_str()), input.length(), hash.data());
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
