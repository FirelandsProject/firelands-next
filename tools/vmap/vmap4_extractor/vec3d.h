#pragma once

// Thin aliases so ported vmap4_extractor sources can keep using Vec3D / AaBox3D
// names retained from the Firelands vmap port.

#include "../common/Vec3.h"
#include "../common/Mat3.h"

using Vec3D     = Firelands::VMap::Vec3;
using AaBox3D   = Firelands::VMap::AaBox3;
using Quaternion = Firelands::VMap::Quaternion;

inline Vec3D fixCoordSystem(Vec3D const& v) {
    return Firelands::VMap::FixClientCoord(v);
}

inline Vec3D fixCoords(Vec3D const& v) {
    return Firelands::VMap::FixWorldPlacement(v);
}
