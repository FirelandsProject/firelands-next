#ifndef FIRELANDS_SHARED_BIGINT_H
#define FIRELANDS_SHARED_BIGINT_H

#include <openssl/bn.h>
#include <vector>
#include <string>
#include <memory>

namespace Firelands {

    class BigInt {
    public:
        BigInt() : _bn(BN_new()) {}
        explicit BigInt(uint32 val) : _bn(BN_new()) { BN_set_word(_bn, val); }
        explicit BigInt(const std::vector<uint8_t>& bytes) : _bn(BN_new()) {
            BN_bin2bn(bytes.data(), bytes.size(), _bn);
        }
        
        // Copy/Move
        BigInt(const BigInt& other) : _bn(BN_dup(other._bn)) {}
        BigInt(BigInt&& other) noexcept : _bn(other._bn) { other._bn = nullptr; }
        
        BigInt& operator=(const BigInt& other) {
            if (this != &other) {
                if (_bn) BN_free(_bn);
                _bn = BN_dup(other._bn);
            }
            return *this;
        }

        ~BigInt() { if (_bn) BN_free(_bn); }

        static BigInt ModExp(const BigInt& base, const BigInt& exp, const BigInt& mod) {
            BigInt res;
            BN_CTX* ctx = BN_CTX_new();
            BN_mod_exp(res._bn, base._bn, exp._bn, mod._bn, ctx);
            BN_CTX_free(ctx);
            return res;
        }

        static BigInt ModMul(const BigInt& a, const BigInt& b, const BigInt& mod) {
            BigInt res;
            BN_CTX* ctx = BN_CTX_new();
            BN_mod_mul(res._bn, a._bn, b._bn, mod._bn, ctx);
            BN_CTX_free(ctx);
            return res;
        }

        static BigInt ModAdd(const BigInt& a, const BigInt& b, const BigInt& mod) {
            BigInt res;
            BN_CTX* ctx = BN_CTX_new();
            BN_mod_add(res._bn, a._bn, b._bn, mod._bn, ctx);
            BN_CTX_free(ctx);
            return res;
        }

        std::vector<uint8_t> ToBinary(int32 size = -1) const {
            int n = BN_num_bytes(_bn);
            if (size != -1 && size > n) n = size;
            std::vector<uint8_t> res(n, 0);
            BN_bn2bin(_bn, res.data() + (n - BN_num_bytes(_bn)));
            return res;
        }

        BIGNUM* GetRaw() { return _bn; }

    private:
        BigInt(BIGNUM* bn) : _bn(bn) {}
        BIGNUM* _bn;
    };

} // namespace Firelands

#endif // FIRELANDS_SHARED_BIGINT_H
