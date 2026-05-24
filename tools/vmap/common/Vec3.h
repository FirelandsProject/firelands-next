#pragma once

// Minimal math types for the vmap pipeline.
// Originally ported from reference implementation vec3d.h
// and from G3D::AABox layout expected by the binary format.
// Intentionally stand-alone — no G3D or Boost dependency.

#include <bit>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace Firelands::VMap {

// ─────────────────────────── Vec3 ────────────────────────────────────────────

struct Vec3 {
    float x{}, y{}, z{};

    constexpr Vec3() = default;
    constexpr Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    constexpr Vec3 operator+(Vec3 const& v) const { return {x + v.x, y + v.y, z + v.z}; }
    constexpr Vec3 operator-(Vec3 const& v) const { return {x - v.x, y - v.y, z - v.z}; }
    constexpr Vec3 operator*(float d)       const { return {x * d,   y * d,   z * d};   }
    constexpr float operator*(Vec3 const& v) const { return x*v.x + y*v.y + z*v.z; }   // dot
    constexpr Vec3 operator%(Vec3 const& v) const {                                      // cross
        return {y*v.z - z*v.y, z*v.x - x*v.z, x*v.y - y*v.x};
    }

    Vec3& operator+=(Vec3 const& v) { x += v.x; y += v.y; z += v.z; return *this; }
    Vec3& operator-=(Vec3 const& v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
    Vec3& operator*=(float d)       { x *= d;   y *= d;   z *= d;   return *this; }

    float  lengthSq() const { return x*x + y*y + z*z; }
    float  length()   const { return std::sqrt(x*x + y*y + z*z); }

    Vec3& normalize() { *this *= 1.0f / length(); return *this; }

    // Index access (matches G3D::Vector3::operator[] convention used in BIH).
    float  operator[](int i) const { return (&x)[i]; }
    float& operator[](int i)       { return (&x)[i]; }

    // Axis of maximum extent (used by BIH build to pick split axis).
    int primaryAxis() const {
        float ax = std::abs(x), ay = std::abs(y), az = std::abs(z);
        if (ax >= ay && ax >= az) return 0;
        if (ay >= az) return 1;
        return 2;
    }

    explicit operator float*()       { return &x; }
    explicit operator const float*() const { return &x; }

    // G3D-style fuzzy comparisons used by BIH.
    static bool fuzzyEq(float a, float b)  { return std::abs(a - b) < 1e-6f; }
    static bool fuzzyNe(float a, float b)  { return std::abs(a - b) >= 1e-6f; }
};

inline Vec3 operator*(float d, Vec3 const& v) { return v * d; }

// Coordinate system fixes (must match reference exactly).
// fixClientCoord: applied to M2 bounding vertices at load.
inline Vec3 FixClientCoord(Vec3 const& v) { return {v.x, v.z, -v.y}; }
// fixWorldPlacement: applied to WMO/doodad world positions from MDDF/MODF.
inline Vec3 FixWorldPlacement(Vec3 const& v) { return {v.z, v.x, v.y}; }

// ─────────────────────────── AaBox3 ──────────────────────────────────────────

struct AaBox3 {
    Vec3 lo{};
    Vec3 hi{};

    AaBox3() = default;
    AaBox3(Vec3 const& lo_, Vec3 const& hi_) : lo(lo_), hi(hi_) {}

    void merge(AaBox3 const& o) {
        lo.x = std::min(lo.x, o.lo.x);
        lo.y = std::min(lo.y, o.lo.y);
        lo.z = std::min(lo.z, o.lo.z);
        hi.x = std::max(hi.x, o.hi.x);
        hi.y = std::max(hi.y, o.hi.y);
        hi.z = std::max(hi.z, o.hi.z);
    }

    /// Expand the box to contain point `p` (used by vmap4 assembler bounds merge).
    void merge(Vec3 const& p) {
        lo.x = std::min(lo.x, p.x);
        lo.y = std::min(lo.y, p.y);
        lo.z = std::min(lo.z, p.z);
        hi.x = std::max(hi.x, p.x);
        hi.y = std::max(hi.y, p.y);
        hi.z = std::max(hi.z, p.z);
    }

    // Used in G3D-style code: bounds.low() / bounds.high()
    Vec3 const& low()  const { return lo; }
    Vec3 const& high() const { return hi; }
    Vec3&       low()        { return lo; }
    Vec3&       high()       { return hi; }

    bool contains(Vec3 const& p) const {
        return p.x >= lo.x && p.x <= hi.x &&
               p.y >= lo.y && p.y <= hi.y &&
               p.z >= lo.z && p.z <= hi.z;
    }

    AaBox3& operator+=(Vec3 const& offset) { lo += offset; hi += offset; return *this; }

    void set(Vec3 const& lo_, Vec3 const& hi_) { lo = lo_; hi = hi_; }
};

// ─────────────────────────── Quaternion ──────────────────────────────────────

struct Quaternion {
    float X{}, Y{}, Z{}, W{1.f};
};

// ─────────────────────────── Helpers ─────────────────────────────────────────

// Bit-cast float ↔ uint32 (avoids UB; used by BIH node encoding).
inline uint32_t FloatToRawBits(float f) { return std::bit_cast<uint32_t>(f); }
inline float RawBitsToFloat(uint32_t i) { return std::bit_cast<float>(i); }

inline float FInf()  { return std::numeric_limits<float>::infinity(); }
inline float FNan()  { return std::numeric_limits<float>::quiet_NaN(); }
inline float FPi()   { return 3.14159265358979323846f; }

} // namespace Firelands::VMap
