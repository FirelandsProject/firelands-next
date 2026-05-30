#ifndef FIRELANDS_INFRASTRUCTURE_COLLISION_WORLD_MODEL_RUNTIME_H
#define FIRELANDS_INFRASTRUCTURE_COLLISION_WORLD_MODEL_RUNTIME_H

#include <application/ports/IMapCollisionQueries.h>
#include <cstdint>
#include <string>
#include <vector>

namespace Firelands {

struct AaBox3 {
  Vec3 lo;
  Vec3 hi;
};

struct MeshTriangle {
  uint32_t idx0 = 0;
  uint32_t idx1 = 0;
  uint32_t idx2 = 0;
};

struct ModelSpawn {
  uint8_t flags = 0;
  uint8_t adtId = 0;
  uint32_t ID = 0;
  Vec3 iPos;
  Vec3 iRot;
  float iScale = 1.0f;
  AaBox3 iBound;
  std::string name;
};

class GroupModelRuntime {
public:
  bool Read(std::vector<uint8_t> const& data, size_t& offset);
  bool RayIntersects(Vec3 const& rayStart, Vec3 const& rayDir,
                     float maxDist, float& hitDist) const;
  float GetHeightAt(float x, float y) const;
  AaBox3 const& GetBounds() const { return _bound; }
  bool RayTriangleIntersect(Vec3 const& orig, Vec3 const& dir,
                            Vec3 const& v0, Vec3 const& v1,
                            Vec3 const& v2, float& t) const;

private:
  AaBox3 _bound;
  uint32_t _mogpFlags = 0;
  uint32_t _groupWMOID = 0;
  std::vector<Vec3> _vertices;
  std::vector<MeshTriangle> _triangles;
};

class WorldModelRuntime {
public:
  bool Read(std::vector<uint8_t> const& data);
  uint32_t GetRootWMOID() const { return _rootWMOID; }
  std::vector<GroupModelRuntime> const& GetGroups() const { return _groups; }

private:
  uint32_t _rootWMOID = 0;
  std::vector<GroupModelRuntime> _groups;
};

struct BIHNode {
  static BIHNode Read(uint32_t const* treeData, uint32_t index);
  bool IsLeaf() const { return axis == 3; }
  uint32_t axis = 0;
  uint32_t childOffset = 0;
  uint32_t leafCount = 0;
  bool bvh2 = false;
  float planeL = 0.0f;
  float planeR = 0.0f;
};

class BoundingIntervalHierarchy {
public:
  bool Read(std::vector<uint8_t> const& data, size_t& offset);
  bool ReadFromData(uint8_t const* data, size_t dataSize);
  void RayIntersect(std::vector<Vec3> const& vertices,
                    std::vector<MeshTriangle> const& triangles,
                    Vec3 const& rayStart, Vec3 const& rayDir,
                    float maxDist, float& hitDist, bool stopAtFirst) const;
  float GetHeightAt(std::vector<Vec3> const& vertices,
                    std::vector<MeshTriangle> const& triangles,
                    float x, float y) const;
  AaBox3 const& GetBounds() const { return _bounds; }
  uint32_t GetObjectCount() const { return _objCount; }
  uint32_t GetObjectIndex(uint32_t i) const { return _objects[i]; }

private:
  AaBox3 _bounds;
  std::vector<uint32_t> _tree;
  std::vector<uint32_t> _objects;
  uint32_t _objCount = 0;

  float RayIntersectInternal(uint32_t const* tree, std::vector<Vec3> const& verts,
                             std::vector<MeshTriangle> const& tris,
                             Vec3 const& rayStart, Vec3 const& rayDir,
                             float maxDist, float& hitDist,
                             bool stopAtFirst, uint32_t nodeOffset) const;
  void BuildRecursive(uint32_t const* tree, std::vector<uint32_t>& stacked,
                      uint32_t nodeOffset) const;
};

} // namespace Firelands

#endif
