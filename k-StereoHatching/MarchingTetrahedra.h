#pragma once

#include "MetaballSdf.h"
#include "MetaballCharacters.h"
#include "HatchVertex.h"
#include <Egg/Math/Float3.h>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <algorithm>
#include <cmath>

// CPU tessellator turning a CharacterSdf into an indexed triangle mesh.
//
// Uses marching TETRAHEDRA (Freudenthal/Kuhn's 6-tet cube split along the
// main diagonal), not classic marching cubes with its 256-entry edge/tri
// lookup tables -- deliberately, to avoid transcribing a large table by hand
// (a single wrong entry silently corrupts geometry) when a from-scratch,
// directly-verifiable case analysis (a tet only has 2^4=16 corner-sign
// patterns, collapsing to 4 simple cases by popcount) gives the same
// category of result: a seamless, sub-cell-resolution smooth surface. This
// also mirrors this codebase's own g-Distance project, which already
// extracts its surface via per-tetrahedron cases rather than cube cases.
//
// Every cell (i,j,k) is split into 6 tets sharing the (i,j,k)-(i+1,j+1,k+1)
// diagonal (the standard 6-permutation Freudenthal split); every crossed
// edge -- including the cube-face diagonals this split introduces -- is
// welded by its two GLOBAL grid-corner indices, which is well-defined and
// automatically consistent across neighboring cells because every cell uses
// the identical diagonal convention (always low-to-high corner).
namespace Hatch {

	namespace Detail {

		// index = x + 2y + 4z
		static const int CornerOffsets[8][3] = {
			{0,0,0}, {1,0,0}, {0,1,0}, {1,1,0},
			{0,0,1}, {1,0,1}, {0,1,1}, {1,1,1}
		};

		// 6 tets, each a permutation-of-axes sweep from corner 0 to corner 7
		// (the cube's main diagonal) -- see file header.
		static const int TetCorners[6][4] = {
			{0,1,3,7},
			{0,1,5,7},
			{0,2,3,7},
			{0,2,6,7},
			{0,4,5,7},
			{0,4,6,7},
		};

		inline uint64_t EdgeKey(uint64_t a, uint64_t b) {
			uint64_t lo = std::min(a, b), hi = std::max(a, b);
			return (lo << 32) ^ hi;
		}

	}

	inline void TessellateCharacter(
		const CharacterSdf& sdf,
		const AABB& bbox,
		uint32_t gridRes,
		std::vector<HatchVertex>& outVerts,
		std::vector<uint32_t>& outIndices)
	{
		using namespace Egg::Math;
		using namespace Detail;

		outVerts.clear();
		outIndices.clear();

		float3 extent = bbox.max - bbox.min;
		float maxAxis = std::max(extent.x, std::max(extent.y, extent.z));
		if (maxAxis <= 1e-6f || gridRes < 2) return;
		float cellSize = maxAxis / (float)gridRes;

		int resX = std::max(1, (int)std::ceil(extent.x / cellSize));
		int resY = std::max(1, (int)std::ceil(extent.y / cellSize));
		int resZ = std::max(1, (int)std::ceil(extent.z / cellSize));
		int nx = resX + 1, ny = resY + 1, nz = resZ + 1;

		auto gridPos = [&](int i, int j, int k) -> float3 {
			return bbox.min + float3((float)i, (float)j, (float)k) * cellSize;
		};
		auto gridLinear = [&](int i, int j, int k) -> uint64_t {
			return (uint64_t)i + (uint64_t)nx * ((uint64_t)j + (uint64_t)ny * (uint64_t)k);
		};

		// Cache field values at every grid point up front -- one SDF eval per
		// unique grid point, not per cell-corner (each interior grid point is
		// shared by up to 8 cells).
		std::vector<float> field((size_t)nx * ny * nz);
		for (int k = 0; k < nz; ++k)
			for (int j = 0; j < ny; ++j)
				for (int i = 0; i < nx; ++i)
					field[gridLinear(i, j, k)] = sdf.Evaluate(gridPos(i, j, k)).distance;

		std::unordered_map<uint64_t, uint32_t> edgeToVertex;
		edgeToVertex.reserve(1 << 16);

		// Returns the (welded) output-vertex index for the zero-crossing on
		// the edge between global grid corners (i0,j0,k0)-(i1,j1,k1), whose
		// field values are f0/f1 (opposite sign, checked by the caller).
		auto crossingVertex = [&](int i0, int j0, int k0, float f0,
		                            int i1, int j1, int k1, float f1) -> uint32_t {
			uint64_t key = EdgeKey(gridLinear(i0, j0, k0), gridLinear(i1, j1, k1));
			auto it = edgeToVertex.find(key);
			if (it != edgeToVertex.end()) return it->second;

			float t = f0 / (f0 - f1); // f0,f1 opposite sign => t in (0,1)
			float3 p0 = gridPos(i0, j0, k0);
			float3 p1 = gridPos(i1, j1, k1);
			float3 p = p0 + (p1 - p0) * t;

			SdfSample s = sdf.Evaluate(p);
			float glen = s.gradient.Length();
			float3 n = (glen > 1e-6f) ? s.gradient * (1.0f / glen) : float3(0, 1, 0);

			HatchVertex v;
			v.position = p;
			v.normal = n;
			v.hatchDirection = float3(1, 0, 0); // placeholder -- filled in by CurvatureDirection.h
			v.curvature = 0.0f;

			uint32_t idx = (uint32_t)outVerts.size();
			outVerts.push_back(v);
			edgeToVertex.emplace(key, idx);
			return idx;
		};

		auto emitTri = [&](uint32_t a, uint32_t b, uint32_t c) {
			outIndices.push_back(a); outIndices.push_back(b); outIndices.push_back(c);
		};

		int cellCorner[8][3]; // (i,j,k) of each of this cell's 8 corners, refilled per cell
		float cellField[8];

		for (int k = 0; k < resZ; ++k) {
			for (int j = 0; j < resY; ++j) {
				for (int i = 0; i < resX; ++i) {
					bool anyInside = false, anyOutside = false;
					for (int c = 0; c < 8; ++c) {
						int ci = i + CornerOffsets[c][0];
						int cj = j + CornerOffsets[c][1];
						int ck = k + CornerOffsets[c][2];
						cellCorner[c][0] = ci; cellCorner[c][1] = cj; cellCorner[c][2] = ck;
						float f = field[gridLinear(ci, cj, ck)];
						cellField[c] = f;
						if (f < 0.0f) anyInside = true; else anyOutside = true;
					}
					if (!anyInside || !anyOutside) continue; // whole cell on one side -- no crossing

					for (int t = 0; t < 6; ++t) {
						const int* tc = TetCorners[t];
						float f[4] = { cellField[tc[0]], cellField[tc[1]], cellField[tc[2]], cellField[tc[3]] };
						int insideIdx[4], outsideIdx[4];
						int nIn = 0, nOut = 0;
						for (int c = 0; c < 4; ++c) {
							if (f[c] < 0.0f) insideIdx[nIn++] = c; else outsideIdx[nOut++] = c;
						}
						if (nIn == 0 || nIn == 4) continue;

						auto cornerIJK = [&](int localTetCorner, int& ci, int& cj, int& ck) {
							int cornerId = tc[localTetCorner];
							ci = cellCorner[cornerId][0]; cj = cellCorner[cornerId][1]; ck = cellCorner[cornerId][2];
						};
						auto edgeVert = [&](int localA, int localB) -> uint32_t {
							int ai, aj, ak, bi, bj, bk;
							cornerIJK(localA, ai, aj, ak);
							cornerIJK(localB, bi, bj, bk);
							return crossingVertex(ai, aj, ak, f[localA], bi, bj, bk, f[localB]);
						};

						if (nIn == 1) {
							int a = insideIdx[0];
							uint32_t p0 = edgeVert(a, outsideIdx[0]);
							uint32_t p1 = edgeVert(a, outsideIdx[1]);
							uint32_t p2 = edgeVert(a, outsideIdx[2]);
							emitTri(p0, p1, p2);
						} else if (nIn == 3) {
							int d = outsideIdx[0];
							uint32_t p0 = edgeVert(d, insideIdx[0]);
							uint32_t p1 = edgeVert(d, insideIdx[1]);
							uint32_t p2 = edgeVert(d, insideIdx[2]);
							emitTri(p0, p2, p1); // reversed winding vs. the nIn==1 case
						} else { // nIn == 2
							int a = insideIdx[0], b = insideIdx[1];
							int c = outsideIdx[0], d = outsideIdx[1];
							uint32_t pac = edgeVert(a, c);
							uint32_t pad = edgeVert(a, d);
							uint32_t pbd = edgeVert(b, d);
							uint32_t pbc = edgeVert(b, c);
							emitTri(pac, pad, pbd);
							emitTri(pac, pbd, pbc);
						}
					}
				}
			}
		}
	}

}
