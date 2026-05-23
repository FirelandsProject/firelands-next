#pragma once

// Quaternion → rotation matrix and Euler XYZ extraction.
// Matrix row layout and quaternion convention match G3D
// `deps/g3dlite/source/Matrix3.cpp` (Watt & Watt) so doodad-set extraction
// stays numerically aligned with the reference vmap4_extractor.

#include "../common/Mat3.h"

namespace Firelands::VMap::Vmap4 {

// Build rotation matrix from a *unit* quaternion (x, y, z, w).
Mat3 Mat3FromQuaternion(float x, float y, float z, float w);

// G3D `Matrix3::toEulerAnglesXYZ` (standard G3D convention).
// OUT order matches the reference call-site mapping onto Vec3 fields:
//   rfXAngle → rotation.z, rfYAngle → rotation.x, rfZAngle → rotation.y
void Mat3ToEulerAnglesXYZ_G3D(const Mat3& m, float& outZ, float& outX, float& outY);

inline float DegToRad(float d) {
    return d * (3.14159265358979323846f / 180.f);
}

inline float RadToDeg(float r) {
    return r * (180.f / 3.14159265358979323846f);
}

} // namespace Firelands::VMap::Vmap4
