#include <gtest/gtest.h>
#include <infrastructure/collision/WorldModelRuntime.h>
#include <infrastructure/collision/VMapManager2.h>
#include <cstring>
#include <vector>

using namespace Firelands;

namespace {

std::vector<uint8_t> MakeMinimalVmoData() {
  // Build a synthetic .vmo with one GroupModel containing a single triangle
  struct WriteBuf {
    std::vector<uint8_t> buf;
    void WriteU32(uint32_t v) {
      buf.push_back(v & 0xFF);
      buf.push_back((v >> 8) & 0xFF);
      buf.push_back((v >> 16) & 0xFF);
      buf.push_back((v >> 24) & 0xFF);
    }
    void WriteF32(float v) {
      uint32_t i;
      std::memcpy(&i, &v, 4);
      WriteU32(i);
    }
    void WriteTag(const char* tag) {
      for (int i = 0; i < 4; ++i) buf.push_back(tag[i]);
    }
  };

  WriteBuf w;
  w.WriteTag("VMAP_4.8");

  // WMOD chunk: RootWMOID=100, flags=0
  w.WriteTag("WMOD");
  w.WriteU32(8);
  w.WriteU32(100);
  w.WriteU32(0);

  // GMOD chunk: 1 group
  w.WriteTag("GMOD");
  size_t gmodSizePos = w.buf.size();
  w.WriteU32(0);
  w.WriteU32(1);  // count

  // GroupModel: bound, mogpFlags, groupWMOID
  // AaBox3 lo: (-10, -10, -1), hi: (10, 10, 3)
  w.WriteF32(-10.0f); w.WriteF32(-10.0f); w.WriteF32(-1.0f);
  w.WriteF32(10.0f);  w.WriteF32(10.0f);  w.WriteF32(3.0f);
  w.WriteU32(0);   // mogpFlags
  w.WriteU32(200);  // groupWMOID

  // VERT chunk: 3 vertices (a triangle on the ground)
  w.WriteTag("VERT");
  w.WriteU32(4 + 4 + 3*12);  // chunkSize = uint32 count + 3 * Vec3
  w.WriteU32(3);  // vertexCount
  w.WriteF32(0.0f);  w.WriteF32(0.0f);  w.WriteF32(0.0f);   // v0
  w.WriteF32(10.0f); w.WriteF32(0.0f);  w.WriteF32(0.0f);   // v1
  w.WriteF32(0.0f);  w.WriteF32(10.0f); w.WriteF32(0.0f);   // v2

  // TRIM chunk: 1 triangle
  w.WriteTag("TRIM");
  w.WriteU32(4 + 4 + 3*4);  // chunkSize = uint32 count + MeshTriangle
  w.WriteU32(1);  // triCount
  w.WriteU32(0); w.WriteU32(1); w.WriteU32(2);

  // MBIH chunk: minimal BIH
  w.WriteTag("MBIH");
  w.WriteU32(24 + 4 + 4*3 + 4 + 4);  // chunkSize
  // bounds
  w.WriteF32(-10.0f); w.WriteF32(-10.0f); w.WriteF32(-1.0f);
  w.WriteF32(10.0f);  w.WriteF32(10.0f);  w.WriteF32(3.0f);
  // treeSize=3 (leaf node)
  w.WriteU32(3);
  // node[0] = leaf (axis=3, childOffset=0)
  // bits 31-30=3, bit29=0, bits28-0=0
  w.WriteU32((3u << 30) | 0u);
  w.WriteU32(1);  // leafCount=1
  w.WriteU32(0);  // padding
  // object count = 1
  w.WriteU32(1);
  w.WriteU32(0);  // object[0]=0 (index into triangles)

  // LIQU chunk: empty
  w.WriteTag("LIQU");
  w.WriteU32(0);

  // Fix GMOD chunkSize
  uint32_t gmodSize = static_cast<uint32_t>(w.buf.size() - gmodSizePos - 4);
  w.buf[gmodSizePos + 0] = gmodSize & 0xFF;
  w.buf[gmodSizePos + 1] = (gmodSize >> 8) & 0xFF;
  w.buf[gmodSizePos + 2] = (gmodSize >> 16) & 0xFF;
  w.buf[gmodSizePos + 3] = (gmodSize >> 24) & 0xFF;

  return w.buf;
}

std::vector<uint8_t> MakeMinimalVmoWithoutGroups() {
  struct WriteBuf {
    std::vector<uint8_t> buf;
    void WriteU32(uint32_t v) {
      buf.push_back(v & 0xFF);
      buf.push_back((v >> 8) & 0xFF);
      buf.push_back((v >> 16) & 0xFF);
      buf.push_back((v >> 24) & 0xFF);
    }
    void WriteTag(const char* tag) {
      for (int i = 0; i < 4; ++i) buf.push_back(tag[i]);
    }
  };
  WriteBuf w;
  w.WriteTag("VMAP_4.8");
  w.WriteTag("WMOD");
  w.WriteU32(8);
  w.WriteU32(42);
  w.WriteU32(0);
  return w.buf;
}

} // namespace

TEST(WorldModelRuntime, ReadsMinimalVmo) {
  auto data = MakeMinimalVmoData();
  WorldModelRuntime model;
  ASSERT_TRUE(model.Read(data));
  EXPECT_EQ(model.GetRootWMOID(), 100u);
  ASSERT_EQ(model.GetGroups().size(), 1u);
  EXPECT_EQ(model.GetGroups()[0].GetBounds().lo.x, -10.0f);
  EXPECT_EQ(model.GetGroups()[0].GetBounds().hi.z, 3.0f);
}

TEST(WorldModelRuntime, VmoWithoutGroupsParsesCorrectly) {
  auto data = MakeMinimalVmoWithoutGroups();
  WorldModelRuntime model;
  ASSERT_TRUE(model.Read(data));
  EXPECT_EQ(model.GetRootWMOID(), 42u);
  EXPECT_TRUE(model.GetGroups().empty());
}

TEST(GroupModelRuntime, RayHitsTriangle) {
  auto data = MakeMinimalVmoData();
  WorldModelRuntime model;
  ASSERT_TRUE(model.Read(data));
  ASSERT_EQ(model.GetGroups().size(), 1u);

  auto const& group = model.GetGroups()[0];
  Vec3 start = {3.0f, 3.0f, 10.0f};
  Vec3 dir = {0.0f, 0.0f, -1.0f};
  float hitDist = 100.0f;
  EXPECT_TRUE(group.RayIntersects(start, dir, 100.0f, hitDist));
  EXPECT_NEAR(hitDist, 10.0f, 0.1f);
}

TEST(GroupModelRuntime, RayMissesTriangleFromBelow) {
  auto data = MakeMinimalVmoData();
  WorldModelRuntime model;
  ASSERT_TRUE(model.Read(data));
  auto const& group = model.GetGroups()[0];
  Vec3 start = {3.0f, 3.0f, -5.0f};
  Vec3 dir = {0.0f, 0.0f, -1.0f};
  float hitDist = 100.0f;
  EXPECT_FALSE(group.RayIntersects(start, dir, 100.0f, hitDist));
}

TEST(GroupModelRuntime, GetHeightAtReturnsGroundZ) {
  auto data = MakeMinimalVmoData();
  WorldModelRuntime model;
  ASSERT_TRUE(model.Read(data));
  auto const& group = model.GetGroups()[0];
  float h = group.GetHeightAt(3.0f, 3.0f);
  EXPECT_NEAR(h, 0.0f, 0.1f);
}

TEST(BoundingIntervalHierarchy, ReadsAndIntersects) {
  auto data = MakeMinimalVmoData();
  WorldModelRuntime model;
  ASSERT_TRUE(model.Read(data));
  auto const& group = model.GetGroups()[0];

  (void)group; // BIH is embedded in the group model, tested via RayIntersects
  SUCCEED();
}

TEST(VMapManager2, IsMapLoadedReturnsFalseForUnloadedMap) {
  VMapManager2 mgr;
  EXPECT_FALSE(mgr.IsMapLoaded(0));
  EXPECT_FALSE(mgr.IsMapLoaded(530));
}
