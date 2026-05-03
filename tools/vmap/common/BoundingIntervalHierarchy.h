#pragma once

// Bounding Interval Hierarchy (BIH) — spatial acceleration structure for
// ray-casting and point queries over triangle meshes and spawn instances.
//
// Originally ported from reference implementation BoundingIntervalHierarchy.h
// Replaces G3D::AABox / G3D::Vector3 / G3D::Ray with our Vec3 / AaBox3 types.
// The binary serialization format (writeToFile / readFromFile) is bit-identical
// to the reference so that vmap files produced here can be read by the runtime.
//
// Algorithm: BIH from Sunflow (MIT/X11), Christopher Kulla 2003-2007.

#include "Vec3.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <vector>

namespace Firelands::VMap {

#define FL_BIH_MAX_STACK 64

// ─── simple Ray (origin + unit direction) ─────────────────────────────────────

struct Ray {
    Vec3 origin;
    Vec3 direction; // must be normalized

    Ray(Vec3 const& o, Vec3 const& d) : origin(o), direction(d) {}
};

// ─── AABound helper (lo/hi pair used during build) ───────────────────────────

struct AABound {
    Vec3 lo{};
    Vec3 hi{};
};

// ─── BIH ─────────────────────────────────────────────────────────────────────

class BIH {
public:
    BIH() { InitEmpty(); }

    // Build over an array of primitives.
    // `getBounds(prim, AaBox3&)` must fill the bounding box for a single prim.
    template<class PrimArray, class BoundsFunc>
    void Build(const PrimArray& primitives, BoundsFunc getBounds,
               uint32_t leafSize = 3, bool printStats = false)
    {
        if (primitives.empty()) {
            InitEmpty();
            return;
        }

        BuildData dat;
        dat.maxPrims = static_cast<int>(leafSize);
        dat.numPrims = static_cast<uint32_t>(primitives.size());
        dat.indices  = new uint32_t[dat.numPrims];
        dat.primBound = new AaBox3[dat.numPrims];

        getBounds(primitives[0], _bounds);
        for (uint32_t i = 0; i < dat.numPrims; ++i) {
            dat.indices[i] = i;
            AaBox3 b;
            getBounds(primitives[i], b);
            dat.primBound[i] = b;
            _bounds.merge(b);
        }

        std::vector<uint32_t> tmpTree;
        BuildStats stats;
        BuildHierarchy(tmpTree, dat, stats);
        if (printStats) stats.Print();

        _objects.resize(dat.numPrims);
        for (uint32_t i = 0; i < dat.numPrims; ++i)
            _objects[i] = dat.indices[i];
        _tree = tmpTree;

        delete[] dat.primBound;
        delete[] dat.indices;
    }

    uint32_t PrimCount() const { return static_cast<uint32_t>(_objects.size()); }

    // Ray intersection.  `intersectCb(ray, objIdx, maxDist, stopAtFirst) -> bool`
    template<typename RayCallback>
    void IntersectRay(const Ray& r, RayCallback& cb, float& maxDist,
                      bool stopAtFirst = false) const
    {
        float iMin = -1.f, iMax = -1.f;
        Vec3 org = r.origin, dir = r.direction;
        Vec3 invDir;

        for (int i = 0; i < 3; ++i) {
            invDir[i] = 1.f / dir[i];
            if (Vec3::fuzzyNe(dir[i], 0.f)) {
                float t1 = (_bounds.lo[i] - org[i]) * invDir[i];
                float t2 = (_bounds.hi[i] - org[i]) * invDir[i];
                if (t1 > t2) std::swap(t1, t2);
                if (t1 > iMin) iMin = t1;
                if (t2 < iMax || iMax < 0.f) iMax = t2;
                if (iMax <= 0 || iMin >= maxDist) return;
            }
        }
        if (iMin > iMax) return;
        iMin = std::max(iMin, 0.f);
        iMax = std::min(iMax, maxDist);

        uint32_t oFront[3], oBack[3], oFront3[3], oBack3[3];
        for (int i = 0; i < 3; ++i) {
            oFront[i]  = FloatToRawBits(dir[i]) >> 31u;
            oBack[i]   = oFront[i] ^ 1u;
            oFront3[i] = oFront[i] * 3;
            oBack3[i]  = oBack[i]  * 3;
            ++oFront[i];
            ++oBack[i];
        }

        StackNode stack[FL_BIH_MAX_STACK];
        int stackPos = 0, node = 0;

        while (true) {
            while (true) {
                uint32_t tn   = _tree[node];
                uint32_t axis = (tn >> 30u) & 3u;
                bool     bvh2 = (tn & (1u << 29u)) != 0;
                int      off  = static_cast<int>(tn & ~(7u << 29u));
                if (!bvh2) {
                    if (axis < 3) {
                        float tf = (RawBitsToFloat(_tree[node + oFront[axis]]) - org[axis]) * invDir[axis];
                        float tb = (RawBitsToFloat(_tree[node + oBack [axis]]) - org[axis]) * invDir[axis];
                        if (tf < iMin && tb > iMax) break;
                        int back = off + static_cast<int>(oBack3[axis]);
                        node = back;
                        if (tf < iMin) { iMin = (tb >= iMin) ? tb : iMin; continue; }
                        node = off + static_cast<int>(oFront3[axis]);
                        if (tb > iMax) { iMax = (tf <= iMax) ? tf : iMax; continue; }
                        stack[stackPos] = {static_cast<uint32_t>(back),
                                           (tb >= iMin) ? tb : iMin, iMax};
                        ++stackPos;
                        iMax = (tf <= iMax) ? tf : iMax;
                        continue;
                    } else {
                        int n = static_cast<int>(_tree[node + 1]);
                        while (n-- > 0) {
                            bool hit = cb(r, _objects[off++], maxDist, stopAtFirst);
                            if (stopAtFirst && hit) return;
                        }
                        break;
                    }
                } else {
                    if (axis > 2) return;
                    float tf = (RawBitsToFloat(_tree[node + oFront[axis]]) - org[axis]) * invDir[axis];
                    float tb = (RawBitsToFloat(_tree[node + oBack [axis]]) - org[axis]) * invDir[axis];
                    node = off;
                    iMin = (tf >= iMin) ? tf : iMin;
                    iMax = (tb <= iMax) ? tb : iMax;
                    if (iMin > iMax) break;
                    continue;
                }
            }
            do {
                if (stackPos == 0) return;
                --stackPos;
                iMin = stack[stackPos].tnear;
                if (maxDist < iMin) continue;
                node = static_cast<int>(stack[stackPos].node);
                iMax = stack[stackPos].tfar;
                break;
            } while (true);
        }
    }

    // Point containment query.  `intersectCb(point, objIdx)`
    template<typename IsectCallback>
    void IntersectPoint(Vec3 const& p, IsectCallback& cb) const
    {
        if (!_bounds.contains(p)) return;

        StackNode stack[FL_BIH_MAX_STACK];
        int stackPos = 0, node = 0;

        while (true) {
            while (true) {
                uint32_t tn   = _tree[node];
                uint32_t axis = (tn >> 30u) & 3u;
                bool     bvh2 = (tn & (1u << 29u)) != 0;
                int      off  = static_cast<int>(tn & ~(7u << 29u));
                if (!bvh2) {
                    if (axis < 3) {
                        float tl = RawBitsToFloat(_tree[node + 1]);
                        float tr = RawBitsToFloat(_tree[node + 2]);
                        if (tl < p[axis] && tr > p[axis]) break;
                        int right = off + 3;
                        node = right;
                        if (tl < p[axis]) continue;
                        node = off;
                        if (tr > p[axis]) continue;
                        stack[stackPos++].node = static_cast<uint32_t>(right);
                        continue;
                    } else {
                        int n = static_cast<int>(_tree[node + 1]);
                        while (n-- > 0) cb(p, _objects[off++]);
                        break;
                    }
                } else {
                    if (axis > 2) return;
                    float tl = RawBitsToFloat(_tree[node + 1]);
                    float tr = RawBitsToFloat(_tree[node + 2]);
                    node = off;
                    if (tl > p[axis] || tr < p[axis]) break;
                    continue;
                }
            }
            if (stackPos == 0) return;
            node = static_cast<int>(stack[--stackPos].node);
        }
    }

    bool WriteToFile(FILE* wf) const;
    bool ReadFromFile(FILE* rf);

protected:
    std::vector<uint32_t> _tree;
    std::vector<uint32_t> _objects;
    AaBox3                _bounds{};

    struct BuildData {
        uint32_t* indices{};
        AaBox3*   primBound{};
        uint32_t  numPrims{};
        int       maxPrims{};
    };

    struct StackNode {
        uint32_t node{};
        float    tnear{};
        float    tfar{};
    };

    class BuildStats {
        int numNodes{}, numLeaves{}, sumObjects{};
        int minObjects{0x0FFFFFFF}, maxObjects{-1};
        int sumDepth{}, minDepth{0x0FFFFFFF}, maxDepth{-1};
        int numLeavesN[6]{};
        int numBVH2{};
    public:
        void UpdateInner()               { ++numNodes; }
        void UpdateBVH2()                { ++numBVH2; }
        void UpdateLeaf(int depth, int n);
        void Print() const;
    };

    void InitEmpty();
    void CreateNode(std::vector<uint32_t>& tree, int idx, uint32_t lo, uint32_t hi) const;
    void BuildHierarchy(std::vector<uint32_t>& tree, BuildData& dat, BuildStats& stats);
    void Subdivide(int left, int right,
                   std::vector<uint32_t>& tree, BuildData& dat,
                   AABound& gridBox, AABound& nodeBox,
                   int nodeIndex, int depth, BuildStats& stats);
};

} // namespace Firelands::VMap
