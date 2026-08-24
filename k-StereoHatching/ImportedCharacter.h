#pragma once

#include "HatchVertex.h"
#include "CurvatureDirection.h"
#include "MetaballCharacters.h"
#include <Egg/Common.h>
#include <Egg/Mesh/Geometry.h>
#include <Egg/Math/Float3.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <cstdint>

// A scene made of a real imported mesh (e.g. an FBX character) instead of a
// procedural metaball SDF. The rest of the pipeline (hatchGeometryVS/PS,
// crossProjectVS/PS) only cares about HatchVertex's four fields, so an
// imported mesh just needs to fill them the same way the SDF path does --
// position/normal come straight from the file, and hatchDirection/curvature
// come from a discrete one-ring least-squares curvature-tensor fit (no
// analytic SDF gradient/Hessian exists for an arbitrary imported mesh, so
// CurvatureDirection.h's numerical-Hessian approach doesn't apply here --
// only its shared tensor-to-direction math, Detail::PrincipalDirectionFromTensor,
// is reused).
namespace Hatch {

	namespace Detail {

		// Per-vertex principal curvature from mesh connectivity alone: for
		// each one-ring edge e = neighbor - p, the normal curvature along e
		// is estimated as kn = 2*dot(n,e)/dot(e,e) (Taubin's normal-curvature
		// estimator), then a tangent-plane quadratic form kn(x,y) = a*x^2 +
		// 2*b*x*y + c*y^2 is least-squares fit across all one-ring edges
		// (x,y = edge direction projected into the (t1,t2) tangent basis).
		inline void ComputeMeshCurvature(std::vector<HatchVertex>& verts, const std::vector<uint32_t>& indices) {
			using namespace Egg::Math;

			size_t n = verts.size();
			std::vector<std::vector<uint32_t>> neighbors(n);
			for (size_t i = 0; i + 2 < indices.size(); i += 3) {
				uint32_t tri[3] = { indices[i], indices[i + 1], indices[i + 2] };
				for (int e = 0; e < 3; ++e) {
					neighbors[tri[e]].push_back(tri[(e + 1) % 3]);
					neighbors[tri[e]].push_back(tri[(e + 2) % 3]);
				}
			}
			for (size_t vi = 0; vi < n; ++vi) {
				std::vector<uint32_t>& nb = neighbors[vi];
				std::sort(nb.begin(), nb.end());
				nb.erase(std::unique(nb.begin(), nb.end()), nb.end());
			}

			for (size_t vi = 0; vi < n; ++vi) {
				HatchVertex& v = verts[vi];
				const float3& p = v.position;
				const float3& nrm = v.normal;

				float3 t1, t2;
				BranchlessOnb(nrm, t1, t2);

				const std::vector<uint32_t>& nb = neighbors[vi];
				double M[3][3] = { {0,0,0},{0,0,0},{0,0,0} };
				double rhs[3] = { 0,0,0 };
				int usable = 0;

				for (size_t k = 0; k < nb.size(); ++k) {
					float3 e = verts[nb[k]].position - p;
					float elen2 = e.Dot(e);
					if (elen2 < 1e-10f) continue;
					float kn = 2.0f * nrm.Dot(e) / elen2;

					float3 et = e - nrm * nrm.Dot(e);
					float etLen = et.Length();
					if (etLen < 1e-6f) continue;
					float3 dHat = et * (1.0f / etLen);

					double x = dHat.Dot(t1), y = dHat.Dot(t2);
					double f[3] = { x * x, 2.0 * x * y, y * y };
					for (int r = 0; r < 3; ++r) {
						rhs[r] += f[r] * kn;
						for (int c = 0; c < 3; ++c) M[r][c] += f[r] * f[c];
					}
					++usable;
				}

				float a = 0.0f, b = 0.0f, c = 0.0f;
				if (usable >= 3) {
					// Solve M*(a,b,c) = rhs via Gaussian elimination with partial pivoting.
					double A[3][4];
					for (int r = 0; r < 3; ++r) { for (int cc = 0; cc < 3; ++cc) A[r][cc] = M[r][cc]; A[r][3] = rhs[r]; }
					bool ok = true;
					for (int col = 0; col < 3 && ok; ++col) {
						int piv = col;
						for (int r = col + 1; r < 3; ++r) if (std::fabs(A[r][col]) > std::fabs(A[piv][col])) piv = r;
						if (std::fabs(A[piv][col]) < 1e-9) { ok = false; break; }
						if (piv != col) { for (int cc = 0; cc < 4; ++cc) std::swap(A[piv][cc], A[col][cc]); }
						for (int r = 0; r < 3; ++r) {
							if (r == col) continue;
							double f = A[r][col] / A[col][col];
							for (int cc = col; cc < 4; ++cc) A[r][cc] -= f * A[col][cc];
						}
					}
					if (ok) {
						a = (float)(A[0][3] / A[0][0]);
						b = (float)(A[1][3] / A[1][1]);
						c = (float)(A[2][3] / A[2][2]);
					}
				}

				PrincipalCurvature pc = PrincipalDirectionFromTensor(t1, t2, a, b, c);
				v.hatchDirection = pc.dir;
				v.curvature = pc.kappa;
			}
		}

	}

	// Loads every mesh in an assimp-supported file (concatenated into one
	// indexed HatchVertex buffer), flattens the whole scene-graph transform
	// hierarchy into the vertices (PreTransformVertices -- static hatching
	// doesn't need the skeleton), then rescales/recenters it to stand
	// targetHeight tall with its feet at y=0, comparable to the procedural
	// characters' scale.
	inline Egg::Mesh::IndexedGeometry::P LoadMeshCharacter(ID3D12Device* device, const std::string& relativeMediaPath, float targetHeight = 1.9f) {
		Assimp::Importer importer;
		std::string path = "../Media/" + relativeMediaPath;
		const aiScene* scene = importer.ReadFile(path,
			aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
			aiProcess_GenSmoothNormals | aiProcess_PreTransformVertices);

		ASSERT(scene != nullptr, "Failed to load mesh file: '%s'. Assimp error: '%s'", path.c_str(), importer.GetErrorString());
		ASSERT(scene->HasMeshes(), "Mesh file '%s' has no meshes.", path.c_str());

		std::vector<HatchVertex> verts;
		std::vector<uint32_t> indices;

		for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
			const aiMesh* mesh = scene->mMeshes[m];
			uint32_t baseIndex = (uint32_t)verts.size();
			for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
				HatchVertex v;
				v.position = Egg::Math::float3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
				Egg::Math::float3 nrm(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
				v.normal = (nrm.Length() > 1e-8f) ? nrm.Normalize() : Egg::Math::float3(0, 1, 0);
				v.hatchDirection = Egg::Math::float3(1, 0, 0);
				v.curvature = 0.0f;
				verts.push_back(v);
			}
			for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
				const aiFace& face = mesh->mFaces[f];
				if (face.mNumIndices != 3) continue;
				indices.push_back(baseIndex + face.mIndices[0]);
				indices.push_back(baseIndex + face.mIndices[1]);
				indices.push_back(baseIndex + face.mIndices[2]);
			}
		}
		ASSERT(!verts.empty() && !indices.empty(), "Mesh file '%s' produced no triangles.", path.c_str());

		Egg::Math::float3 mn(1e9f, 1e9f, 1e9f), mx(-1e9f, -1e9f, -1e9f);
		for (size_t i = 0; i < verts.size(); ++i) {
			const Egg::Math::float3& p = verts[i].position;
			mn.x = std::min(mn.x, p.x); mn.y = std::min(mn.y, p.y); mn.z = std::min(mn.z, p.z);
			mx.x = std::max(mx.x, p.x); mx.y = std::max(mx.y, p.y); mx.z = std::max(mx.z, p.z);
		}
		float height = std::max(mx.y - mn.y, 1e-4f);
		float scale = targetHeight / height;
		Egg::Math::float3 center((mn.x + mx.x) * 0.5f, mn.y, (mn.z + mx.z) * 0.5f);
		for (size_t i = 0; i < verts.size(); ++i)
			verts[i].position = (verts[i].position - center) * scale;

		Detail::ComputeMeshCurvature(verts, indices);

		return Egg::Mesh::IndexedGeometry::Create(device,
			verts.data(), (unsigned int)(verts.size() * sizeof(HatchVertex)), (unsigned int)sizeof(HatchVertex),
			indices.data(), (unsigned int)(indices.size() * sizeof(uint32_t)), DXGI_FORMAT_R32_UINT);
	}

	// A handful of scattered placements so the instances don't read as a
	// robotic grid -- same shared geometry drawn multiple times with
	// different translation-only world offsets, exactly like the procedural
	// characters. `sdf` is left default-constructed and unused (only
	// worldOffset is read for this scene -- see StereoHatchingApp.h's
	// RebuildCharacters/PopulateCommandList, which never call Character::sdf
	// for imported-mesh characters).
	inline std::vector<Character> BuildNoseyKnightPlacements() {
		using Egg::Math::float3;
		std::vector<Character> placements;
		placements.push_back({ CharacterSdf{}, float3(-2.4f, 0.0f,  0.00f), "Nosey Knight 1" });
		placements.push_back({ CharacterSdf{}, float3(-0.8f, 0.0f,  0.30f), "Nosey Knight 2" });
		placements.push_back({ CharacterSdf{}, float3(0.8f,  0.0f, -0.20f), "Nosey Knight 3" });
		placements.push_back({ CharacterSdf{}, float3(2.4f,  0.0f,  0.15f), "Nosey Knight 4" });
		return placements;
	}

}
