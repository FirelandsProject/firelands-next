#include <infrastructure/collision/WorldModelRuntime.h>

#include <cmath>
#include <cstring>
#include <vector>

namespace Firelands {

namespace {

uint32_t ReadU32(uint8_t const* data, size_t& offset) {
  uint32_t val = 0;
  std::memcpy(&val, data + offset, 4);
  offset += 4;
  return val;
}

float ReadFloat(uint8_t const* data, size_t& offset) {
  uint32_t bits = ReadU32(data, offset);
  float f = 0.0f;
  std::memcpy(&f, &bits, 4);
  return f;
}

Vec3 ReadVec3(uint8_t const* data, size_t& offset) {
  Vec3 v;
  v.x = ReadFloat(data, offset);
  v.y = ReadFloat(data, offset);
  v.z = ReadFloat(data, offset);
  return v;
}

Vec3 Cross(Vec3 const& a, Vec3 const& b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
          a.x * b.y - a.y * b.x};
}

float Dot(Vec3 const& a, Vec3 const& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 Sub(Vec3 const& a, Vec3 const& b) {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 Normalize(Vec3 const& v) {
  float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
  if (len < 1e-8f)
    return {0, 0, 1};
  return {v.x / len, v.y / len, v.z / len};
}

bool RayAABBIntersect(Vec3 const& orig, Vec3 const& dir, AaBox3 const& box,
                      float& tNear, float& tFar) {
  tNear = -1e30f;
  tFar = 1e30f;

  float t1 = (box.lo.x - orig.x) / (dir.x + 1e-8f);
  float t2 = (box.hi.x - orig.x) / (dir.x + 1e-8f);
  if (t1 > t2)
    std::swap(t1, t2);
  tNear = std::max(tNear, t1);
  tFar = std::min(tFar, t2);
  if (tNear > tFar)
    return false;

  t1 = (box.lo.y - orig.y) / (dir.y + 1e-8f);
  t2 = (box.hi.y - orig.y) / (dir.y + 1e-8f);
  if (t1 > t2)
    std::swap(t1, t2);
  tNear = std::max(tNear, t1);
  tFar = std::min(tFar, t2);
  if (tNear > tFar)
    return false;

  t1 = (box.lo.z - orig.z) / (dir.z + 1e-8f);
  t2 = (box.hi.z - orig.z) / (dir.z + 1e-8f);
  if (t1 > t2)
    std::swap(t1, t2);
  tNear = std::max(tNear, t1);
  tFar = std::min(tFar, t2);
  return tNear <= tFar;
}

} // namespace

// --- GroupModelRuntime ---

bool GroupModelRuntime::Read(std::vector<uint8_t> const& data, size_t& offset) {
  uint8_t const* buf = data.data();
  size_t const size = data.size();

  _bound.lo = ReadVec3(buf, offset);
  _bound.hi = ReadVec3(buf, offset);
  if (offset + 8 > size)
    return false;
  _mogpFlags = ReadU32(buf, offset);
  _groupWMOID = ReadU32(buf, offset);

  while (offset + 4 <= size) {
    char tag[5] = {};
    std::memcpy(tag, buf + offset, 4);
    offset += 4;

    if (offset + 4 > size)
      return false;
    uint32_t chunkSize = ReadU32(buf, offset);
    size_t chunkEnd = offset + chunkSize;

    if (chunkEnd > size)
      return false;

    if (std::memcmp(tag, "VERT", 4) == 0) {
      uint32_t vertCount = ReadU32(buf, offset);
      _vertices.resize(vertCount);
      for (uint32_t i = 0; i < vertCount; ++i)
        _vertices[i] = ReadVec3(buf, offset);
    } else if (std::memcmp(tag, "TRIM", 4) == 0) {
      uint32_t triCount = ReadU32(buf, offset);
      _triangles.resize(triCount);
      for (uint32_t i = 0; i < triCount; ++i) {
        _triangles[i].idx0 = ReadU32(buf, offset);
        _triangles[i].idx1 = ReadU32(buf, offset);
        _triangles[i].idx2 = ReadU32(buf, offset);
      }
    } else if (std::memcmp(tag, "MBIH", 4) == 0) {
      // Embedded BIH: skip for now, use brute force triangle test
      offset = chunkEnd;
    } else if (std::memcmp(tag, "LIQU", 4) == 0) {
      offset = chunkEnd;
    } else {
      offset = chunkEnd;
    }
  }
  return true;
}

bool GroupModelRuntime::RayTriangleIntersect(Vec3 const& orig, Vec3 const& dir,
                                              Vec3 const& v0, Vec3 const& v1,
                                              Vec3 const& v2, float& t) const {
  constexpr float kEpsilon = 1e-8f;
  Vec3 e1 = Sub(v1, v0);
  Vec3 e2 = Sub(v2, v0);
  Vec3 pvec = Cross(dir, e2);
  float det = Dot(e1, pvec);

  if (std::abs(det) < kEpsilon)
    return false;
  float invDet = 1.0f / det;
  Vec3 tvec = Sub(orig, v0);
  float u = Dot(tvec, pvec) * invDet;
  if (u < 0.0f || u > 1.0f)
    return false;
  Vec3 qvec = Cross(tvec, e1);
  float v = Dot(dir, qvec) * invDet;
  if (v < 0.0f || u + v > 1.0f)
    return false;
  t = Dot(e2, qvec) * invDet;
  return t > kEpsilon;
}

bool GroupModelRuntime::RayIntersects(Vec3 const& rayStart, Vec3 const& rayDir,
                                       float maxDist, float& hitDist) const {
  hitDist = maxDist;
  bool hit = false;
  for (auto const& tri : _triangles) {
    if (tri.idx0 >= _vertices.size() || tri.idx1 >= _vertices.size() ||
        tri.idx2 >= _vertices.size())
      continue;
    float t = 0.0f;
    if (RayTriangleIntersect(rayStart, rayDir, _vertices[tri.idx0],
                             _vertices[tri.idx1], _vertices[tri.idx2], t)) {
      if (t < hitDist) {
        hitDist = t;
        hit = true;
      }
    }
  }
  return hit && hitDist < maxDist;
}

float GroupModelRuntime::GetHeightAt(float x, float y) const {
  Vec3 rayStart = {x, y, 10000.0f};
  Vec3 rayDir = {0.0f, 0.0f, -1.0f};
  float hitDist = 20000.0f;
  if (RayIntersects(rayStart, rayDir, 20000.0f, hitDist))
    return 10000.0f - hitDist;
  return -1e30f;
}

// --- WorldModelRuntime ---

bool WorldModelRuntime::Read(std::vector<uint8_t> const& data) {
  uint8_t const* buf = data.data();
  size_t const size = data.size();
  size_t offset = 8;

  while (offset + 4 <= size) {
    char tag[5] = {};
    std::memcpy(tag, buf + offset, 4);
    offset += 4;

    if (std::memcmp(tag, "WMOD", 4) == 0) {
      if (offset + 4 > size)
        return false;
      uint32_t chunkSize = ReadU32(buf, offset);
      if (offset + 4 > size)
        return false;
      _rootWMOID = ReadU32(buf, offset);
      offset += 4;
    } else if (std::memcmp(tag, "GMOD", 4) == 0) {
      if (offset + 4 > size)
        return false;
      uint32_t chunkSize = ReadU32(buf, offset);
      uint32_t count = ReadU32(buf, offset);
      _groups.resize(count);
      size_t gmodEnd = offset + chunkSize - 4;
      for (uint32_t i = 0; i < count && offset < gmodEnd; ++i) {
        _groups[i].Read(data, offset);
      }
      offset = gmodEnd;
    } else if (std::memcmp(tag, "GBIH", 4) == 0) {
      uint32_t chunkSize = ReadU32(buf, offset);
      offset += chunkSize;
    } else {
      if (offset + 4 > size)
        return false;
      uint32_t chunkSize = ReadU32(buf, offset);
      offset += chunkSize;
    }
  }
  return true;
}

// --- BoundingIntervalHierarchy ---

bool BoundingIntervalHierarchy::Read(std::vector<uint8_t> const& data,
                                      size_t& offset) {
  uint8_t const* buf = data.data();
  size_t const size = data.size();

  if (offset + 36 > size)
    return false;
  _bounds.lo = ReadVec3(buf, offset);
  _bounds.hi = ReadVec3(buf, offset);

  uint32_t treeSize = ReadU32(buf, offset);
  _tree.resize(treeSize);
  for (uint32_t i = 0; i < treeSize; ++i) {
    _tree[i] = ReadU32(buf, offset);
  }

  _objCount = ReadU32(buf, offset);
  _objects.resize(_objCount);
  for (uint32_t i = 0; i < _objCount; ++i) {
    _objects[i] = ReadU32(buf, offset);
  }
  return true;
}

bool BoundingIntervalHierarchy::ReadFromData(uint8_t const* data,
                                              size_t dataSize) {
  size_t offset = 0;
  std::vector<uint8_t> wrapper;
  wrapper.assign(data, data + dataSize);
  return Read(wrapper, offset);
}

void BoundingIntervalHierarchy::BuildRecursive(
    uint32_t const* tree, std::vector<uint32_t>& stacked,
    uint32_t nodeOffset) const {
  if (nodeOffset + 2 >= _tree.size())
    return;

  BIHNode node = BIHNode::Read(tree, nodeOffset);
  if (node.IsLeaf()) {
    for (uint32_t i = 0; i < node.leafCount; ++i) {
      uint32_t idx = node.childOffset + i;
      if (idx < _objects.size())
        stacked.push_back(_objects[idx]);
    }
  } else {
    BuildRecursive(tree, stacked, nodeOffset + 3);
    if (node.bvh2) {
      BuildRecursive(tree, stacked, nodeOffset + 9);
    } else {
      BuildRecursive(tree, stacked, node.childOffset);
    }
  }
}

float BoundingIntervalHierarchy::RayIntersectInternal(
    uint32_t const* tree, std::vector<Vec3> const& verts,
    std::vector<MeshTriangle> const& tris, Vec3 const& rayStart,
    Vec3 const& rayDir, float maxDist, float& hitDist, bool stopAtFirst,
    uint32_t nodeOffset) const {

  if (nodeOffset + 2 >= _tree.size())
    return hitDist;

  BIHNode node = BIHNode::Read(tree, nodeOffset);

  if (node.IsLeaf()) {
    GroupModelRuntime tmp;
    for (uint32_t i = 0; i < node.leafCount; ++i) {
      uint32_t idx = node.childOffset + i;
      if (idx >= _objects.size())
        continue;
      uint32_t triIdx = _objects[idx];
      if (triIdx >= tris.size())
        continue;
      auto const& tri = tris[triIdx];
      if (tri.idx0 >= verts.size() || tri.idx1 >= verts.size() ||
          tri.idx2 >= verts.size())
        continue;
      float t = 0.0f;
      if (tmp.RayTriangleIntersect(rayStart, rayDir, verts[tri.idx0],
                                   verts[tri.idx1], verts[tri.idx2], t)) {
        if (t > 0.0f && t < hitDist) {
          hitDist = t;
          if (stopAtFirst)
            return hitDist;
        }
      }
    }
    return hitDist;
  }

  float planeDist = node.bvh2 ? node.planeL : 0.0f;
  float startDist = 0.0f;
  float endDist = maxDist;

  if (node.axis < 3) {
    float origin = (&rayStart.x)[node.axis];
    float direction = (&rayDir.x)[node.axis];
    if (std::abs(direction) > 1e-8f) {
      startDist = (planeDist - origin) / direction;
      endDist = startDist;
    } else {
      if (origin < planeDist) {
        startDist = -1e30f;
        endDist = 1e30f;
      }
    }
  }

  uint32_t nearChild = nodeOffset + 3;
  uint32_t farChild = node.bvh2 ? nodeOffset + 9 : node.childOffset;

  if (startDist >= 0.0f || endDist < 0.0f) {
    hitDist = RayIntersectInternal(tree, verts, tris, rayStart, rayDir, maxDist,
                                    hitDist, stopAtFirst, nearChild);
    if (stopAtFirst && hitDist < maxDist)
      return hitDist;
    hitDist = RayIntersectInternal(tree, verts, tris, rayStart, rayDir, maxDist,
                                    hitDist, stopAtFirst, farChild);
  } else {
    hitDist = RayIntersectInternal(tree, verts, tris, rayStart, rayDir, maxDist,
                                    hitDist, stopAtFirst, farChild);
    if (stopAtFirst && hitDist < maxDist)
      return hitDist;
    hitDist = RayIntersectInternal(tree, verts, tris, rayStart, rayDir, maxDist,
                                    hitDist, stopAtFirst, nearChild);
  }
  return hitDist;
}

void BoundingIntervalHierarchy::RayIntersect(
    std::vector<Vec3> const& vertices,
    std::vector<MeshTriangle> const& triangles, Vec3 const& rayStart,
    Vec3 const& rayDir, float maxDist, float& hitDist,
    bool stopAtFirst) const {
  RayIntersectInternal(_tree.data(), vertices, triangles, rayStart, rayDir,
                       maxDist, hitDist, stopAtFirst, 0);
}

float BoundingIntervalHierarchy::GetHeightAt(
    std::vector<Vec3> const& vertices,
    std::vector<MeshTriangle> const& triangles, float x, float y) const {

  Vec3 rayStart = {x, y, _bounds.hi.z + 100.0f};
  Vec3 rayDir = {0.0f, 0.0f, -1.0f};
  float hitDist = (_bounds.hi.z - _bounds.lo.z) + 200.0f;

  RayIntersectInternal(_tree.data(), vertices, triangles, rayStart, rayDir,
                       hitDist, hitDist, true, 0);
  if (hitDist < (_bounds.hi.z - _bounds.lo.z) + 200.0f)
    return _bounds.hi.z + 100.0f - hitDist;
  return -1e30f;
}

BIHNode BIHNode::Read(uint32_t const* treeData, uint32_t index) {
  BIHNode node;
  uint32_t header = treeData[index];
  node.axis = (header >> 30) & 0x3;
  node.bvh2 = (header >> 29) & 0x1;
  node.childOffset = header & 0x1FFFFFFF;

  if (node.IsLeaf()) {
    node.leafCount = treeData[index + 1];
  } else if (node.bvh2) {
    std::memcpy(&node.planeL, &treeData[index + 1], 4);
    std::memcpy(&node.planeR, &treeData[index + 2], 4);
  }
  return node;
}

} // namespace Firelands
