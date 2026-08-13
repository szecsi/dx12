#pragma once
#include "Egg/Common.h"
#include <Egg/SimpleApp.h>
#include <Egg/Utility.h>
#include <Egg/Shader.h>
#include <Egg/ConstantBuffer.hpp>
#include <Egg/Cam/FirstPerson.h>
#include <Egg/Compute/ComputeShader.h>
#include <Egg/Compute/FenceChain.h>

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>

#include "Shaders/DistanceConfig.hlsli"
#include "Shaders/DistanceCb.hlsli"
#include "Shaders/DistanceGridCb.hlsli"
#include "Shaders/DistanceFrameCb.hlsli"
#include "Shaders/PickedTetCb.hlsli"
#include "Shaders/TorusListCb.hlsli"

#include <vector>
#include <string>
#include <cstdio>

// Must match TorusListCb.ShapeKind's convention and rasterLabelCS.hlsl's
// branch on it. 0/1 (Torus/Ellipsoid) are analytic-SDF shapes, resolved via
// TorusSdf.hlsli as before. 2-6 are discrete grid-index patterns with no
// analytic SDF at all -- deliberately pathological "ambiguous cube" stress
// tests (a lone voxel; a straight line; two label-1 voxels touching only
// along a cube's face diagonal; touching only along a cube's body diagonal;
// a one-voxel-thick square slab) -- rasterLabelCS.hlsl sets their labels
// directly from tid, bypassing ShapeSd/torii entirely. No meaningful
// raymarch reference exists for these (torii stays empty), so the
// background stays visible behind them.
enum DistanceTestShape {
	TestShape_Torus = 0,
	TestShape_Ellipsoid = 1,
	TestShape_SinglePoint = 2,
	TestShape_Line = 3,
	TestShape_DiagonalLine2D = 4,
	TestShape_DiagonalLine3D = 5,
	TestShape_Slab = 6,
	// Single torus, exactly 2 labels (0=background, 1=torus) anywhere in the
	// domain -- reuses the SAME analytic-SDF raymarch path as TestShape_Torus
	// on the GPU (ShapeKind<=1), just with only 1 entry in torusCb.data.torii
	// instead of 3. Added specifically to test smoothnessJacobiCS.hlsl's
	// current two-label-only Term 1 simplification (the "other label is
	// 1-Li" assumption) without any 3+-label ambiguity in the way -- made
	// the default test shape for that reason.
	TestShape_SingleTorus = 7,
	// A single fully-filled A-cube: the 8 A-nodes at grid-midpoint +{0,1} on
	// every axis all get label 1 -- the smallest possible genuinely-3D solid
	// feature (one whole cube's worth of tets uniformly interior), as
	// opposed to Line/DiagonalLine3D's 1-node-wide curves or Slab's 1-node-
	// thick sheet.
	TestShape_Box2x2x2 = 8,
};

// g-Distance: replaces g-Aequor's particle relaxation with a direct scalar
// optimization over per-label nodal potentials on a BCC lattice. A-sublattice
// = original cubic grid (fixed input labels); B-sublattice = cube centers
// (solved). Every node holds <=8 candidate-label raw potentials; within each
// disphenoid tet (2 A + 2 B nodes) a label's potential is affine, so the
// argmax-label decomposition is planar per tet with interface normal
// grad(phi_i)-grad(phi_j) (deliberately unnormalized, keeping the whole
// energy quadratic in the unknowns). See the approved plan
// (soft-stargazing-biscuit.md) for the full design rationale.
class DistanceApp : public Egg::SimpleApp {

	static constexpr float CellSize = CELL_SIZE; // must match DistanceConfig.hlsli -- physical unit, independent of grid resolution
	static constexpr uint MaxIncidentTets = MAX_INCIDENT_TETS;

	// -- runtime grid-size state --
	// GridRes is a GUI slider now (applied on Reinitialize, capped at 256 --
	// see the approved plan's memory-scaling discussion for why not higher),
	// not a compile-time constant. gridResSetting/windowCubeDimSetting are
	// the freely-draggable GUI values; GridRes/BDim/ACount/BCount/NodeCount/
	// CubeOriginMin/CubeBoundDim/TotalCubeCandidates/TetCount/WindowCubeDim/
	// WindowTetCount/windowOriginCube* are the CURRENTLY APPLIED values,
	// matching whatever's actually allocated right now -- recomputed by
	// EnsureGridBuffersSized(), called once at the top of RunReinit() (never
	// on Continue). Keeping these separate from the GUI setting means
	// dragging the slider without hitting Reinitialize can never desync
	// buffer sizes from what these members claim. Defaults below match
	// GridRes=20's old compile-time values exactly, so anything that
	// (incorrectly) read them before the first Reinit would still see
	// today's numbers.
	int gridResSetting = 20;        // GUI: "Grid Resolution" (applied on Reinitialize)
	// Real (grid-unit) full width of the rendered window -- 16 is the user's
	// own original suggestion, and also happens to exactly reproduce
	// WindowCubeDim=40 (today's whole-domain q-dispatch size) at the default
	// GridRes=20 via the 2*W+8 formula below (2*16+8=40), so the default
	// case stays a true no-op.
	int windowCubeDimSetting = 21;  // GUI: "Render Window Size" (real grid units, applied on Reinitialize)

	uint GridRes = 20, BDim = 19, ACount = 8000, BCount = 6859, NodeCount = 14859;
	// Rhombohedral cube-based tet indexing -- computed on the fly
	// everywhere, no connectivity buffers at all. See DistanceLattice.hlsli's
	// header comment for the bccToRhombo derivation. Must match that file
	// exactly (same "must match" convention as GridRes/CellSize/
	// MaxIncidentTets above).
	int  CubeOriginMin = -1;
	uint CubeBoundDim = 40, TotalCubeCandidates = 64000;
	// Total tet SLOTS -- fixed/dense indexing (tetBase = 6*cubeLinearIndex).
	// No storage anywhere for the whole domain's worth of these; every
	// tetIndex is computed fresh. Must match DistanceLattice.hlsli exactly.
	uint TetCount = 384000;

	// Render window: GridRes-independent, fixed-size region actually
	// extracted/rendered (surfaceVertexBuffer's real sizing driver) --
	// origin centered on the domain center (same center BuildShapeList()
	// places test shapes at), see EnsureGridBuffersSized().
	int  windowOriginCubeX = 0, windowOriginCubeY = 0, windowOriginCubeZ = 0;
	uint WindowCubeDim = 40, WindowTetCount = 384000, WindowRealHalfExtent = 10;

	// 0 = "nothing applied yet" sentinel, forces EnsureGridBuffersSized()'s
	// first call to allocate everything.
	int lastAppliedGridRes = 0, lastAppliedWindowCubeDim = 0;

	uint RasterGroups = 0, BuildCandidatesAGroups = 0, BuildCandidatesBGroups = 0,
		ExtractSurfaceGroups = 0, SmoothnessGroups = 0, CommitGroups = 0;
	uint BlockSmoothingTilesPerAxis = 0; // ceil(GridRes/2) -- smoothnessJacobiBlockCS.hlsl's 2x2x2(A)+2x2x2(B) tile dispatch, one tile per axis-group

	float MaxMarchDist = 60.0f;
	static constexpr int MaxMarchSteps = 256;

	uint frameCount = 0;

	Egg::Cam::FirstPerson::P camera;

protected:
	com_ptr<ID3D12CommandAllocator>    uploadAllocator;
	com_ptr<ID3D12GraphicsCommandList> uploadCommandList;
	Egg::Compute::Fence uploadFence;
	uint64_t uploadFenceValue = 0;

	// -- static lattice/topology data, built once at init (RunReinit only) --
	// Tet connectivity (corners, incident tets, cross-cube neighbors) has NO
	// buffers at all -- every consumer computes it on the fly from a tet or
	// node index via DistanceLattice.hlsli's GetTetCornerQs/ResolveCorner/
	// GatherIncidentTets/GetCrossNeighbors.
	com_ptr<ID3D12Resource> rasterLabelBuffer;          // ACount uint: ground-truth A-node label
	com_ptr<ID3D12Resource> nodeIsConnectingBuffer;     // ACount uint, 2-bit flags: bit0=sole local connector of its same-label neighborhood, bit1=local max of same-label NodeFootDist (ties count), see computeConnectingNodesCS.hlsl
	// JFA (Jump Flooding) feature transform over the A-sublattice -- ping-pong
	// seed-node-index buffers (jfaInitCS/jfaStepCS.hlsl) finalized into a
	// per-A-node distance ("footvector length" to the nearest differently-
	// labeled A-node, jfaFinalizeCS.hlsl), read by buildCandidatesCS.hlsl to
	// seed initial candidate potentials. Init-only, same cadence as
	// rasterLabelBuffer/nodeIsConnectingBuffer.
	com_ptr<ID3D12Resource> jfaSeedBufferA;             // ACount uint
	com_ptr<ID3D12Resource> jfaSeedBufferB;             // ACount uint
	bool jfaFinalIsBufferA = false;                     // which of jfaSeedBufferA/B held the last Reinit's converged seeds -- set by RunTopologyBuild, read by ReadBackPickedTetDiagnostics for debug purposes
	com_ptr<ID3D12Resource> nodeFootDistBuffer;         // ACount float

	// -- per-node candidate/potential state, (re)seeded at init, evolved by the outer loop --
	com_ptr<ID3D12Resource> nodeCandidateLabelBuffer;   // NodeCount*2 uint, MAX_CANDIDATES(8) 8-bit labels packed per node (SENTINEL_CANDIDATE = unused slot)
	com_ptr<ID3D12Resource> nodePotentialBuffer;        // NodeCount*MAX_CANDIDATES float, "current" (Jacobi read buffer)
	com_ptr<ID3D12Resource> nodePotentialScratchBuffer; // NodeCount*MAX_CANDIDATES float, Jacobi write buffer
	com_ptr<ID3D12Resource> surfaceVertexBuffer;        // TetCount*6 SurfaceVertex (pos+normal+labelI+labelJ), render-only
	// Snapshot of each node's winning candidate label, taken once at the
	// start of each outer round (snapshotWinnerCS.hlsl) -- Term 1's edge
	// activity/pair determination reads this FROZEN value, not the live
	// (every-sweep-changing) winner, matching the old TetInterfacePair
	// scheme's "freeze combinatorics for one round" cadence.
	com_ptr<ID3D12Resource> nodeFrozenWinnerBuffer;     // NodeCount uint

	// -- volume floor (connecting nodes only, see smoothnessJacobiCS.hlsl) --
	com_ptr<ID3D12Resource> nodeCurrentVolumeBuffer;    // NodeCount float: node's own winning-label reconstructed volume, written by smoothnessJacobiCS every sweep

	Egg::Compute::ComputeShader rasterLabelCS;
	Egg::Compute::ComputeShader computeConnectingNodesCS;
	Egg::Compute::ComputeShader jfaInitCS;
	Egg::Compute::ComputeShader jfaStepCS;
	Egg::Compute::ComputeShader jfaFinalizeCS;
	Egg::Compute::ComputeShader buildCandidatesCS;
	Egg::Compute::ComputeShader snapshotWinnerCS;
	Egg::Compute::ComputeShader smoothnessJacobiCS;
	Egg::Compute::ComputeShader smoothnessJacobiBlockCS; // Term-1-only tile/groupshared reimplementation, see smoothnessJacobiBlockCS.hlsl
	Egg::Compute::ComputeShader commitPotentialCS;
	Egg::Compute::ComputeShader extractSurfaceCS;

	com_ptr<ID3D12RootSignature> raymarchRootSig;
	com_ptr<ID3D12PipelineState> raymarchPso;
	com_ptr<ID3D12RootSignature> nodePointRootSig;
	com_ptr<ID3D12PipelineState> nodePointPso;
	com_ptr<ID3D12RootSignature> surfaceRootSig;
	com_ptr<ID3D12PipelineState> surfacePso;
	com_ptr<ID3D12RootSignature> wireframeRootSig;
	com_ptr<ID3D12PipelineState> wireframePso;
	com_ptr<ID3D12RootSignature> footSliceRootSig;
	com_ptr<ID3D12PipelineState> footSlicePso;

	Egg::ConstantBuffer<DistanceFrameCb> frameCb;
	Egg::ConstantBuffer<DistanceCb>      distanceCb;
	Egg::ConstantBuffer<DistanceGridCb>  distanceGridCb;
	Egg::ConstantBuffer<TorusListCb>     torusCb;
	Egg::ConstantBuffer<PickedTetCb>     pickedTetCb;

	com_ptr<ID3D12DescriptorHeap> imguiSrvHeap;

	int testShapeKind = TestShape_SingleTorus;
	bool needsReinit = false;
	bool needsContinue = false;
	bool dataValid = false; // true once a Reinit/Continue has actually produced node data

	int iterations = 0;          // outer Lloyd-loop rounds on Reinitialize
	int continueIterations = 1;  // outer Lloyd-loop rounds on Continue
	int jacobiSweepsPerRound = 4; // inner linear-solve depth per outer round

	float smoothnessWeight = 1.0f;
	float marginWeight = 1.0f;
	float marginTarget = 0.5f;
	float regularizerWeight = 0.02f;
	float jacobiDiagEpsilon = 0.05f;
	float seedJitter = 0.05f;
	float ownLabelSeed = 1.0f;
	bool neutralBSeed = true; // B-nodes seed with pure jitter instead of a majority-vote confidence boost, see buildCandidatesCS.hlsl
	bool edgeConnectivityOnly = true; // 18-connectivity (vs 26-connectivity) for computeConnectingNodesCS.hlsl's local flood-fill
	float maxPotentialStep = 0.002f; // hard per-sweep step clamp, see DistanceCb.hlsli -- user-confirmed stable value; 0.1 was not, likely because the all-edges connectivity change couples each unknown to more neighbors than the original fan-only gather did
	float volumeWeight = 5000.0f; // energy term 4 weight, see DistanceCb.hlsli -- needs to be this large (not ~1 like the other weights) to actually outweigh smoothness's gradient at a topologically point-like feature; see "Volume Weight" slider comment
	float volumeFloor = 1.0f; // the floor itself for connecting nodes, see DistanceCb.hlsli
	bool useEdgeWalkTraversal = false; // Term 1 pair-listing method: edge-walking vs. node-adjacent-tets, see smoothnessJacobiCS.hlsl
	bool useBlockSmoothing = false; // experimental: replace smoothnessJacobiCS entirely with the tile/groupshared Term-1-only smoothnessJacobiBlockCS.hlsl for the Jacobi sweep loop (Terms 2-5 are NOT applied while this is on)
	int blockSmoothingSweepCounter = 0; // advances every block-smoothing sweep; RotationOffset = counter % 8 (see smoothnessJacobiBlockCS.hlsl Stage 1)
	float missingFallback = -0.75f; // TetFieldGrad's GetCornerPotential missing-candidate fallback, see DistanceCb.hlsli
	float distanceWeight = 0.0f; // term 5 (distance/Eikonal shaping) weight, see DistanceCb.hlsli -- default 0, off until tuned
	float pointRadiusPx = 3.0f;
	float potentialSizeScale = 1.0f; // winning-potential reference value that maps to full-size sprites, see nodePointVS.hlsl
	float nodeFadeExponent = 1.0f;      // exponential depth-fade rate for distant nodes, see nodePointVS.hlsl
	float nodeFadeStartDistance = 0.5f; // (normalized) camera distance within which nodes stay fully solid, see nodePointVS.hlsl

	// -- tet-picking debug tool (PerformPick()) --
	int mouseX = 0, mouseY = 0;
	bool pickRequested = false;
	bool pickedTetValid = false;
	uint pickedTetIndex = 0;
	uint pickedTetNodes[4] = { 0, 0, 0, 0 }; // A0,A1,B0,B1 global node indices
	bool pickedDiagnosticsValid = false;
	uint pickedInterfaceLabelI = 0, pickedInterfaceLabelJ = 0;
	uint pickedCornerLabels[4][MAX_CANDIDATES] = {};
	float pickedCornerPots[4][MAX_CANDIDATES] = {};
	float pickedCornerCurrentVolume[4] = {};   // NodeCurrentVolume, written by smoothnessJacobiCS's Term 4
	uint pickedCornerIsConnecting[4] = {};     // NodeIsConnecting (A-nodes only; 0 for B corners), computeConnectingNodesCS
	float pickedCornerFootDist[4] = {};        // NodeFootDist (A-nodes only; -1 for B corners), jfaFinalizeCS -- raw, unlike pot[]'s own/suppressed sign convention
	uint pickedCornerFootSeed[4] = {};         // raw converged JFA seed node index (A-nodes only), the input jfaFinalizeCS turned into pickedCornerFootDist -- SENTINEL_LABEL or self-index here is the smoking gun for a JFA propagation bug

	bool showNodes = false;
	bool showSurface = true;
	bool showReference = true;
	bool hideUniformNodes = false; // hide nodes whose structural neighbors all share its current label

	// Debug: cross-sectional plane colored by NodePotential's candidate slots
	// #0 (red channel) / #1 (green channel), see footSliceVS.hlsl/
	// footSlicePS.hlsl -- lets you directly SEE the solved potential field
	// instead of only reading numbers back one tet at a time via the Picked
	// Tet panel. Originally a NodeFootDist/JFA view, repurposed.
	bool showPotentialSlice = false;
	int sliceAxis = 2;             // 0=X, 1=Y, 2=Z
	int sliceIndex = 10;           // grid index along sliceAxis, clamped to [0,GridRes-1] at draw time
	float potentialColorScale = 1.0f; // potential value mapped to full channel brightness

	// Test shape: same torus/ellipsoid scene as g-BCC/g-Aequor, centered in
	// the middle of the (positive-only) BCC lattice index space rather than
	// at the origin -- this lattice reuses g-BCC's APos/BPos convention
	// (indices start at 0), not g-Aequor's centered-at-origin one.
	void BuildShapeList() {
		using namespace Egg::Math;
		float3 center(GridRes * CellSize * 0.5f, GridRes * CellSize * 0.5f, GridRes * CellSize * 0.5f);

		torusCb.data.ShapeKind = (uint)testShapeKind;

		if (testShapeKind == TestShape_SingleTorus) {
			// Reuses the exact ShapeKind<=1 analytic-SDF raymarch path --
			// the GPU doesn't care how many torii are in the list, so no
			// new rasterLabelCS.hlsl branch is needed, just override the
			// uploaded ShapeKind to the value that path actually checks for.
			torusCb.data.ShapeKind = (uint)TestShape_Torus;
			torusCb.data.nTorii = 1;
			torusCb.data.torii[0].center      = center;
			torusCb.data.torii[0].axis        = float3(0, 1, 0).Normalize();
			torusCb.data.torii[0].majorRadius = 6.0f;
			torusCb.data.torii[0].minorRadius = 2.5f;
			torusCb.data.torii[0].label       = 1;
			return;
		}

		if (testShapeKind == TestShape_Ellipsoid) {
			torusCb.data.nTorii = 1;
			torusCb.data.torii[0].center      = center;
			torusCb.data.torii[0].axis        = float3(7.0f, 5.0f, 4.0f);
			torusCb.data.torii[0].majorRadius = 0.0f;
			torusCb.data.torii[0].minorRadius = 0.0f;
			torusCb.data.torii[0].label       = 1;
			return;
		}

		if (testShapeKind >= TestShape_SinglePoint) {
			// No analytic SDF -- rasterLabelCS.hlsl sets these labels
			// directly from grid index (ShapeKind >= 2 branch), torii stays
			// empty (no raymarch reference makes sense for a lone voxel, a
			// 1-voxel-wide diagonal line, or a 1-voxel-thick slab anyway).
			torusCb.data.nTorii = 0;
			return;
		}

		// Restored multi-label test scene: 3 interlocking tori on
		// perpendicular axes, ported from g-BCC's BuildShapeList (which has
		// this exact list, scaled for its 48-unit grid, with two of the
		// three entries commented out -- see g-BCC/BccApp.h). Torus 0/1
		// share this scene's center on perpendicular axes, producing a
		// genuine 3-label junction where they cross; torus 2 sits below,
		// offset and on a third axis, only touching torus 0. Scaled down
		// (roughly by major-radius ratio, 6/14) to fit g-Distance's smaller
		// 20-unit grid -- torus 0's radii match this project's original
		// single-torus default exactly, torus 1/2 keep g-BCC's relative
		// proportions rather than an exact rescale.
		//
		// Note (multi-label caveat, see mwd.tex's Discussion): each tet's
		// active pair is only ever its own top-2 most frequent corner
		// labels (smoothnessJacobiCS.hlsl Term 1's per-edge derivation,
		// extractSurfaceCS.hlsl's independent per-tet rendering pick) as a
		// v1 simplification -- at a genuine 3-label junction like torus
		// 0/1's crossing, no single pair can represent all 3 simultaneously
		// present labels at once.
		struct { float3 offset, axis; float major, minor; uint label; } list[] = {
			{ float3(0, 0, 0),  float3(0, 1, 0), 6.0f, 2.5f, 1 },
			{ float3(0, 0, 0),  float3(1, 0, 0), 4.5f, 2.0f, 2 },
			{ float3(0, -4, 0), float3(0, 0, 1), 3.5f, 1.5f, 3 },
		};

		uint n = (uint)(sizeof(list) / sizeof(list[0]));
		torusCb.data.nTorii = n;
		for (uint i = 0; i < n; i++) {
			torusCb.data.torii[i].center      = center + list[i].offset;
			torusCb.data.torii[i].axis        = list[i].axis.Normalize();
			torusCb.data.torii[i].majorRadius = list[i].major;
			torusCb.data.torii[i].minorRadius = list[i].minor;
			torusCb.data.torii[i].label       = list[i].label;
		}
	}

	void UploadDistanceCb() {
		distanceCb.data.SmoothnessWeight = smoothnessWeight;
		distanceCb.data.MarginWeight = marginWeight;
		distanceCb.data.MarginTarget = marginTarget;
		distanceCb.data.RegularizerWeight = regularizerWeight;
		distanceCb.data.JacobiDiagEpsilon = jacobiDiagEpsilon;
		distanceCb.data.SeedJitter = seedJitter;
		distanceCb.data.OwnLabelSeed = ownLabelSeed;
		distanceCb.data.MaxPotentialStep = maxPotentialStep;
		distanceCb.data.VolumeWeight = volumeWeight;
		distanceCb.data.VolumeFloor = volumeFloor;
		distanceCb.data.MissingFallback = missingFallback;
		distanceCb.data.UseEdgeWalkTraversal = useEdgeWalkTraversal ? 1u : 0u;
		distanceCb.data.DistanceWeight = distanceWeight;
		distanceCb.data._pad3a = 0.0f;
		distanceCb.data._pad3b = 0.0f;
		distanceCb.data._pad3c = 0.0f;
		distanceCb.Upload();
	}

	static com_ptr<ID3D12Resource> CreateRawUavBuffer(ID3D12Device* device, UINT64 sizeBytes, const wchar_t* name) {
		com_ptr<ID3D12Resource> res;
		D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(sizeBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		DX_API("create g-Distance raw UAV buffer")
			device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&desc,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				nullptr,
				IID_PPV_ARGS(res.ReleaseAndGetAddressOf()));
		res->SetName(name);
		return res;
	}

	static com_ptr<ID3D12Resource> CreateReadbackBuffer(ID3D12Device* device, UINT64 sizeBytes) {
		com_ptr<ID3D12Resource> res;
		D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(sizeBytes);
		DX_API("create g-Distance readback buffer")
			device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
				D3D12_HEAP_FLAG_NONE,
				&desc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(res.ReleaseAndGetAddressOf()));
		return res;
	}

	void UploadDistanceGridCb() {
		distanceGridCb.data.GridRes = GridRes;
		distanceGridCb.data.BDim = BDim;
		distanceGridCb.data.ACount = ACount;
		distanceGridCb.data.BCount = BCount;
		distanceGridCb.data.NodeCount = NodeCount;
		distanceGridCb.data.CubeOriginMin = CubeOriginMin;
		distanceGridCb.data.CubeBoundDim = CubeBoundDim;
		distanceGridCb.data.TotalCubeCandidates = TotalCubeCandidates;
		distanceGridCb.data.TetCount = TetCount;
		distanceGridCb.data.WindowOriginCubeX = windowOriginCubeX;
		distanceGridCb.data.WindowOriginCubeY = windowOriginCubeY;
		distanceGridCb.data.WindowOriginCubeZ = windowOriginCubeZ;
		distanceGridCb.data.WindowCubeDim = WindowCubeDim;
		distanceGridCb.data.WindowTetCount = WindowTetCount;
		distanceGridCb.data.WindowRealHalfExtent = WindowRealHalfExtent;
		distanceGridCb.data._padGrid1 = 0.0f;
		distanceGridCb.Upload();
	}

	// Recomputes every grid-resolution-derived value from the current GUI
	// settings (gridResSetting/windowCubeDimSetting -- see the "Grid
	// Resolution"/"Render Window Size" sliders), resizing (Reset + recreate)
	// exactly the buffers whose size depends on them if either setting
	// actually changed since the last call (or on the very first call).
	// Called once at the top of RunReinit() -- never on Continue, matching
	// the existing "topology is init-only" convention. If GridRes itself
	// changed (not just the render window), also recenters the camera and
	// recomputes MaxMarchDist, since the domain's physical size just
	// changed -- a same-GridRes Reinit (e.g. just switching test shape)
	// leaves the camera alone.
	void EnsureGridBuffersSized() {
		int newGridRes = gridResSetting < 4 ? 4 : (gridResSetting > 256 ? 256 : gridResSetting);
		int newWindowCubeDim = windowCubeDimSetting < 8 ? 8 : (windowCubeDimSetting > 128 ? 128 : windowCubeDimSetting);

		bool gridChanged = (lastAppliedGridRes != newGridRes);
		bool windowChanged = (lastAppliedWindowCubeDim != newWindowCubeDim);

		if (!gridChanged && !windowChanged) {
			UploadDistanceGridCb(); // cheap, always safe -- nothing else to do
			return;
		}

		GridRes = (uint)newGridRes;
		BDim = GridRes - 1;
		ACount = GridRes * GridRes * GridRes;
		BCount = BDim * BDim * BDim;
		NodeCount = ACount + BCount;

		CubeOriginMin = -1;
		CubeBoundDim = 2 * GridRes;
		TotalCubeCandidates = CubeBoundDim * CubeBoundDim * CubeBoundDim;
		TetCount = TotalCubeCandidates * 6;

		// windowCubeDimSetting is the desired REAL (grid-unit) full width of
		// the rendered region, GridRes-independent. A q-space axis-aligned
		// dispatch box does NOT correspond to a compact real-space region
		// under the bccToRhombo shear (confirmed empirically: an unscaled
		// q-box left a real-space span 1.5x its own size and, worse, an
		// elongated diagonal shape, not a local window at all) -- so
		// WindowCubeDim (the q-space DISPATCH size) is deliberately
		// oversized to safely CONTAIN the desired real box (forward-mapping
		// a real box of full-width W through q=(i+j,i+k,j+k) needs q-space
		// full-width 2W, +margin for safety), and the actual cutoff is an
		// explicit real-space filter (WindowRealHalfExtent, applied in
		// extractSurfaceCS.hlsl's IsWithinRealWindow / this file's
		// CpuIsWithinRealWindow) on top.
		WindowRealHalfExtent = (uint)(newWindowCubeDim / 2);
		WindowCubeDim = (uint)(2 * newWindowCubeDim) + 8;
		if (WindowCubeDim > CubeBoundDim) WindowCubeDim = CubeBoundDim;
		WindowTetCount = WindowCubeDim * WindowCubeDim * WindowCubeDim * 6;

		// Center the render window on the domain center in q-space -- the
		// same physical center BuildShapeList() places test shapes at
		// (i=j=k=GridRes/2 in real grid coordinates maps to
		// q=(GridRes,GridRes,GridRes) via the bccToRhombo transform).
		// Clamped into the valid cube-origin box [CubeOriginMin,
		// CubeOriginMin+CubeBoundDim) -- the naive centerQ-centered offset
		// alone is wrong specifically because that box is NOT symmetric
		// around centerQ (CubeOriginMin=-1 pads only the low side, see
		// DistanceLattice.hlsli's bounding-box comment). At the default
		// GridRes=20 (WindowCubeDim clamped equal to CubeBoundDim, i.e. the
		// window IS the whole domain), the unclamped formula placed the
		// window at [0,40) instead of the valid [-1,39) -- silently
		// skipping the one real layer at origin=-1 while spilling into the
		// invalid origin=39, which CubeLinearIndex rejects, so
		// GlobalTetIndexFromWindowLocal degenerates that whole end -- a
		// permanent hole at exactly one end of every axis, present even
		// at 0 iterations (unrelated to any solve/seeding issue).
		int centerQ = (int)GridRes;
		int lo = CubeOriginMin;
		int hi = CubeOriginMin + (int)CubeBoundDim - (int)WindowCubeDim;
		int rawOrigin = centerQ - (int)(WindowCubeDim / 2);
		int clampedOrigin = (rawOrigin < lo) ? lo : ((rawOrigin > hi) ? hi : rawOrigin);
		windowOriginCubeX = clampedOrigin;
		windowOriginCubeY = clampedOrigin;
		windowOriginCubeZ = clampedOrigin;

		RasterGroups = (GridRes + 3) / 4;
		BuildCandidatesAGroups = (ACount + THREAD_GROUP_SIZE - 1) / THREAD_GROUP_SIZE;
		BuildCandidatesBGroups = (BCount + THREAD_GROUP_SIZE - 1) / THREAD_GROUP_SIZE;
		ExtractSurfaceGroups = (WindowTetCount + THREAD_GROUP_SIZE - 1) / THREAD_GROUP_SIZE;
		SmoothnessGroups = (NodeCount + THREAD_GROUP_SIZE - 1) / THREAD_GROUP_SIZE;
		CommitGroups = (NodeCount * MAX_CANDIDATES + THREAD_GROUP_SIZE - 1) / THREAD_GROUP_SIZE;
		BlockSmoothingTilesPerAxis = (GridRes + 1u) / 2u;

		if (gridChanged) {
			rasterLabelBuffer          = CreateRawUavBuffer(device.Get(), (UINT64)ACount * sizeof(UINT), L"rasterLabelBuffer");
			nodeIsConnectingBuffer     = CreateRawUavBuffer(device.Get(), (UINT64)ACount * sizeof(UINT), L"nodeIsConnectingBuffer");
			jfaSeedBufferA             = CreateRawUavBuffer(device.Get(), (UINT64)ACount * sizeof(UINT), L"jfaSeedBufferA");
			jfaSeedBufferB             = CreateRawUavBuffer(device.Get(), (UINT64)ACount * sizeof(UINT), L"jfaSeedBufferB");
			nodeFootDistBuffer         = CreateRawUavBuffer(device.Get(), (UINT64)ACount * sizeof(float), L"nodeFootDistBuffer");
			nodeCandidateLabelBuffer   = CreateRawUavBuffer(device.Get(), (UINT64)NodeCount * 2 * sizeof(UINT), L"nodeCandidateLabelBuffer");
			nodePotentialBuffer        = CreateRawUavBuffer(device.Get(), (UINT64)NodeCount * MAX_CANDIDATES * sizeof(float), L"nodePotentialBuffer");
			nodePotentialScratchBuffer = CreateRawUavBuffer(device.Get(), (UINT64)NodeCount * MAX_CANDIDATES * sizeof(float), L"nodePotentialScratchBuffer");
			nodeFrozenWinnerBuffer     = CreateRawUavBuffer(device.Get(), (UINT64)NodeCount * sizeof(UINT), L"nodeFrozenWinnerBuffer");
			nodeCurrentVolumeBuffer    = CreateRawUavBuffer(device.Get(), (UINT64)NodeCount * sizeof(float), L"nodeCurrentVolumeBuffer");
		}
		if (gridChanged || windowChanged) {
			surfaceVertexBuffer = CreateRawUavBuffer(device.Get(), (UINT64)WindowTetCount * 6 * sizeof(float) * 8, L"surfaceVertexBuffer"); // pos(3)+normal(3)+labelI+labelJ, see DistanceSurface.hlsli
		}

		UploadDistanceGridCb();

		if (gridChanged) {
			using namespace Egg::Math;
			MaxMarchDist = GridRes * CellSize * 3.0f;
			if (camera) {
				camera->SetProj(0.9f, aspectRatio, 0.1f, MaxMarchDist);
				camera->SetSpeed(0.15f * GridRes);
				float3 center(GridRes * CellSize * 0.5f, GridRes * CellSize * 0.5f, GridRes * CellSize * 0.5f);
				float3 eye = center + float3(0.0f, GridRes * 0.6f, GridRes * -1.0f);
				camera->SetView(eye, (center - eye).Normalize());
			}
		}

		lastAppliedGridRes = newGridRes;
		lastAppliedWindowCubeDim = newWindowCubeDim;
	}

	// Static lattice + topology build: rasterize A, seed candidate labels +
	// potentials. No tet connectivity is built or stored here at all
	// anymore -- every consumer computes it on the fly (see
	// DistanceLattice.hlsli). Independent of the solve's tunables and never
	// changes across outer-loop rounds, so it only ever runs once per
	// Reinitialize (never on Continue).
	void RunTopologyBuild(com_ptr<ID3D12GraphicsCommandList>& cmd) {
		cmd->SetComputeRootSignature(rasterLabelCS.rootSig.Get());
		cmd->SetPipelineState(rasterLabelCS.pso.Get());
		cmd->SetComputeRootConstantBufferView(0, torusCb.GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(1, rasterLabelBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootConstantBufferView(2, distanceGridCb.GetGPUVirtualAddress());
		cmd->Dispatch(RasterGroups, RasterGroups, RasterGroups);
		cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(rasterLabelBuffer.Get()));

		// JFA (Jump Flooding) footvector-length pass: seeds = A-nodes touching
		// a differently-labeled neighbor; propagate nearest seed via halving
		// power-of-two steps (classic JFA schedule, starting from the
		// smallest power of two >= GridRes), ping-ponging jfaSeedBufferA/B;
		// finalize into nodeFootDistBuffer (distance to nearest boundary
		// A-node), read by buildCandidatesCS below to seed initial candidate
		// potentials. See jfaInitCS/jfaStepCS/jfaFinalizeCS.hlsl.
		cmd->SetComputeRootSignature(jfaInitCS.rootSig.Get());
		cmd->SetPipelineState(jfaInitCS.pso.Get());
		cmd->SetComputeRootUnorderedAccessView(0, rasterLabelBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(1, jfaSeedBufferA->GetGPUVirtualAddress());
		cmd->SetComputeRootConstantBufferView(2, distanceGridCb.GetGPUVirtualAddress());
		cmd->Dispatch(RasterGroups, RasterGroups, RasterGroups);
		cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(jfaSeedBufferA.Get()));

		uint32_t jfaN = 1;
		while (jfaN < GridRes) jfaN *= 2; // smallest power of two >= GridRes
		bool jfaCurrentIsA = true; // jfaInitCS just wrote into jfaSeedBufferA
		for (uint32_t step = jfaN / 2; step >= 1; step /= 2) {
			com_ptr<ID3D12Resource>& src = jfaCurrentIsA ? jfaSeedBufferA : jfaSeedBufferB;
			com_ptr<ID3D12Resource>& dst = jfaCurrentIsA ? jfaSeedBufferB : jfaSeedBufferA;
			cmd->SetComputeRootSignature(jfaStepCS.rootSig.Get());
			cmd->SetPipelineState(jfaStepCS.pso.Get());
			cmd->SetComputeRoot32BitConstant(0, step, 0);
			cmd->SetComputeRootUnorderedAccessView(1, src->GetGPUVirtualAddress());
			cmd->SetComputeRootUnorderedAccessView(2, dst->GetGPUVirtualAddress());
			cmd->SetComputeRootConstantBufferView(3, distanceGridCb.GetGPUVirtualAddress());
			cmd->Dispatch(RasterGroups, RasterGroups, RasterGroups);
			cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(dst.Get()));
			jfaCurrentIsA = !jfaCurrentIsA;
		}
		jfaFinalIsBufferA = jfaCurrentIsA;

		{
			com_ptr<ID3D12Resource>& finalSeed = jfaCurrentIsA ? jfaSeedBufferA : jfaSeedBufferB;
			cmd->SetComputeRootSignature(jfaFinalizeCS.rootSig.Get());
			cmd->SetPipelineState(jfaFinalizeCS.pso.Get());
			cmd->SetComputeRootUnorderedAccessView(0, finalSeed->GetGPUVirtualAddress());
			cmd->SetComputeRootUnorderedAccessView(1, nodeFootDistBuffer->GetGPUVirtualAddress());
			cmd->SetComputeRootConstantBufferView(2, distanceGridCb.GetGPUVirtualAddress());
			cmd->Dispatch(RasterGroups, RasterGroups, RasterGroups);
			cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(nodeFootDistBuffer.Get()));
		}

		cmd->SetComputeRootSignature(computeConnectingNodesCS.rootSig.Get());
		cmd->SetPipelineState(computeConnectingNodesCS.pso.Get());
		cmd->SetComputeRoot32BitConstant(0, edgeConnectivityOnly ? 1u : 0u, 0);
		cmd->SetComputeRootUnorderedAccessView(1, rasterLabelBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(2, nodeIsConnectingBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(3, nodeFootDistBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootConstantBufferView(4, distanceGridCb.GetGPUVirtualAddress());
		cmd->Dispatch(BuildCandidatesAGroups, 1, 1); // ACount-based, same as buildCandidatesCS's A dispatch
		cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(nodeIsConnectingBuffer.Get()));

		cmd->SetComputeRootSignature(buildCandidatesCS.rootSig.Get());
		cmd->SetPipelineState(buildCandidatesCS.pso.Get());
		cmd->SetComputeRootConstantBufferView(1, distanceCb.GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(2, rasterLabelBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(3, nodeCandidateLabelBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(4, nodePotentialBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(5, nodeFootDistBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootConstantBufferView(6, distanceGridCb.GetGPUVirtualAddress());
		cmd->SetComputeRoot32BitConstant(0, 0u, 0); // Mode = A-nodes
		cmd->SetComputeRoot32BitConstant(0, neutralBSeed ? 1u : 0u, 1); // NeutralBSeed
		cmd->Dispatch(BuildCandidatesAGroups, 1, 1);
		cmd->SetComputeRoot32BitConstant(0, 1u, 0); // Mode = B-nodes
		cmd->SetComputeRoot32BitConstant(0, neutralBSeed ? 1u : 0u, 1); // NeutralBSeed
		cmd->Dispatch(BuildCandidatesBGroups, 1, 1);
		{
			D3D12_RESOURCE_BARRIER b[] = {
				CD3DX12_RESOURCE_BARRIER::UAV(nodeCandidateLabelBuffer.Get()),
				CD3DX12_RESOURCE_BARRIER::UAV(nodePotentialBuffer.Get()),
			};
			cmd->ResourceBarrier(_countof(b), b);
		}

	}

	// One outer Lloyd-loop round: snapshot each node's current winning label
	// (snapshotWinnerCS, into NodeFrozenWinner) once, then relax the
	// smoothness/margin/regularizer/volume-floor energy for
	// jacobiSweepsPerRound Jacobi sweeps (smoothnessJacobiCS +
	// commitPotentialCS) against that FROZEN snapshot, then repeat. No
	// per-cube combinatorics vote anymore -- smoothnessJacobiCS.hlsl derives
	// each active label pair locally, per edge, from the two endpoints'
	// frozen winners (see its header comment) -- assignInterfacePairsCS.hlsl
	// /TetInterfacePair are gone entirely, which also means `iterations=0`
	// now genuinely shows the raw, unsmoothed seed (no stale/uninitialized
	// combinatorics buffer to read). NodeFrozenWinner still must be
	// snapshotted once even at iterations=0's zero rounds? No -- with zero
	// rounds this loop body (and thus the snapshot) never runs at all, which
	// is correct: there's no smoothing pass to feed a frozen winner to.
	void RunOneRound(com_ptr<ID3D12GraphicsCommandList>& cmd) {
		cmd->SetComputeRootSignature(snapshotWinnerCS.rootSig.Get());
		cmd->SetPipelineState(snapshotWinnerCS.pso.Get());
		cmd->SetComputeRootUnorderedAccessView(0, nodeCandidateLabelBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(1, nodePotentialBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(2, nodeFrozenWinnerBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootConstantBufferView(3, distanceGridCb.GetGPUVirtualAddress());
		cmd->Dispatch(SmoothnessGroups, 1, 1); // NodeCount-based, same bound as smoothnessJacobiCS
		cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(nodeFrozenWinnerBuffer.Get()));

		int sweeps = jacobiSweepsPerRound > 0 ? jacobiSweepsPerRound : 1;
		for (int s = 0; s < sweeps; s++) {
			if (useBlockSmoothing) {
				// Experimental Term-1-only tile/groupshared path -- see
				// smoothnessJacobiBlockCS.hlsl. Terms 2-5 are NOT applied
				// here; NodeCurrentVolume/NodeIsConnecting are left
				// untouched by this path (Term 4's volume readback will show
				// stale/zero values while this is enabled).
				uint32_t rotationOffset = (uint32_t)(blockSmoothingSweepCounter % 8);
				blockSmoothingSweepCounter++;
				cmd->SetComputeRootSignature(smoothnessJacobiBlockCS.rootSig.Get());
				cmd->SetPipelineState(smoothnessJacobiBlockCS.pso.Get());
				cmd->SetComputeRootConstantBufferView(0, distanceCb.GetGPUVirtualAddress());
				cmd->SetComputeRootUnorderedAccessView(1, nodeCandidateLabelBuffer->GetGPUVirtualAddress());
				cmd->SetComputeRootUnorderedAccessView(2, nodePotentialBuffer->GetGPUVirtualAddress());
				cmd->SetComputeRootUnorderedAccessView(3, nodePotentialScratchBuffer->GetGPUVirtualAddress());
				cmd->SetComputeRoot32BitConstant(4, rotationOffset, 0);
				cmd->SetComputeRootConstantBufferView(5, distanceGridCb.GetGPUVirtualAddress());
				cmd->Dispatch(BlockSmoothingTilesPerAxis, BlockSmoothingTilesPerAxis, BlockSmoothingTilesPerAxis);
				cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(nodePotentialScratchBuffer.Get()));
			} else {
				cmd->SetComputeRootSignature(smoothnessJacobiCS.rootSig.Get());
				cmd->SetPipelineState(smoothnessJacobiCS.pso.Get());
				cmd->SetComputeRootConstantBufferView(0, distanceCb.GetGPUVirtualAddress());
				cmd->SetComputeRootUnorderedAccessView(1, nodeCandidateLabelBuffer->GetGPUVirtualAddress());
				cmd->SetComputeRootUnorderedAccessView(2, nodePotentialBuffer->GetGPUVirtualAddress());
				cmd->SetComputeRootUnorderedAccessView(3, nodePotentialScratchBuffer->GetGPUVirtualAddress());
				cmd->SetComputeRootUnorderedAccessView(4, nodeCurrentVolumeBuffer->GetGPUVirtualAddress());
				cmd->SetComputeRootUnorderedAccessView(5, nodeIsConnectingBuffer->GetGPUVirtualAddress());
				cmd->SetComputeRootUnorderedAccessView(6, nodeFrozenWinnerBuffer->GetGPUVirtualAddress());
				cmd->SetComputeRootConstantBufferView(7, distanceGridCb.GetGPUVirtualAddress());
				cmd->Dispatch(SmoothnessGroups, 1, 1);
				{
					D3D12_RESOURCE_BARRIER b[] = {
						CD3DX12_RESOURCE_BARRIER::UAV(nodePotentialScratchBuffer.Get()),
						CD3DX12_RESOURCE_BARRIER::UAV(nodeCurrentVolumeBuffer.Get()),
					};
					cmd->ResourceBarrier(_countof(b), b);
				}
			}

			cmd->SetComputeRootSignature(commitPotentialCS.rootSig.Get());
			cmd->SetPipelineState(commitPotentialCS.pso.Get());
			cmd->SetComputeRootUnorderedAccessView(0, nodePotentialScratchBuffer->GetGPUVirtualAddress());
			cmd->SetComputeRootUnorderedAccessView(1, nodePotentialBuffer->GetGPUVirtualAddress());
			cmd->SetComputeRootConstantBufferView(2, distanceGridCb.GetGPUVirtualAddress());
			cmd->Dispatch(CommitGroups, 1, 1);
			cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(nodePotentialBuffer.Get()));
		}
	}

	void RunRounds(com_ptr<ID3D12GraphicsCommandList>& cmd, int count) {
		int n = count > 0 ? count : 0;
		for (int i = 0; i < n; i++) RunOneRound(cmd);
	}

	// Render-only: marching-tetrahedra surface extraction over the current
	// (converged) potentials -- run once after the solve settles, not per
	// Jacobi sweep, and re-run whenever the solve state actually changes
	// (end of RunReinit/RunContinue), never every frame.
	void RunExtractSurface(com_ptr<ID3D12GraphicsCommandList>& cmd) {
		cmd->SetComputeRootSignature(extractSurfaceCS.rootSig.Get());
		cmd->SetPipelineState(extractSurfaceCS.pso.Get());
		cmd->SetComputeRootUnorderedAccessView(0, nodeCandidateLabelBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(1, nodePotentialBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(2, surfaceVertexBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootConstantBufferView(3, distanceGridCb.GetGPUVirtualAddress());
		cmd->Dispatch(ExtractSurfaceGroups, 1, 1); // one thread per WINDOW tet slot (WindowTetCount), not the whole domain
		cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(surfaceVertexBuffer.Get()));
	}

	// Full reseed: rebuild the test scene, rasterize + rebuild lattice
	// topology from scratch, then relax for `iterations` outer rounds.
	void RunReinit() {
		EnsureGridBuffersSized(); // must run before BuildShapeList (uses GridRes) and before any buffer is touched
		BuildShapeList();
		torusCb.Upload();
		UploadDistanceCb();

		DX_API("reset upload allocator") uploadAllocator->Reset();
		DX_API("reset upload command list") uploadCommandList->Reset(uploadAllocator.Get(), nullptr);
		auto& cmd = uploadCommandList;

		RunTopologyBuild(cmd);
		RunRounds(cmd, iterations);
		RunExtractSurface(cmd);

		DX_API("close upload command list") cmd->Close();
		ID3D12CommandList* lists[] = { cmd.Get() };
		commandQueue->ExecuteCommandLists(1, lists);
		uploadFence.signal(commandQueue, ++uploadFenceValue);
		uploadFence.cpuWait();

		dataValid = true;
	}

	// Continue: no reseed/rebuild, just more outer rounds on top of whatever
	// RunReinit (or an earlier Continue) already left in the node buffers.
	void RunContinue() {
		UploadDistanceCb(); // pick up any slider changes since the last run

		DX_API("reset upload allocator") uploadAllocator->Reset();
		DX_API("reset upload command list") uploadCommandList->Reset(uploadAllocator.Get(), nullptr);
		auto& cmd = uploadCommandList;

		RunRounds(cmd, continueIterations);
		RunExtractSurface(cmd);

		DX_API("close upload command list") cmd->Close();
		ID3D12CommandList* lists[] = { cmd.Get() };
		commandQueue->ExecuteCommandLists(1, lists);
		uploadFence.signal(commandQueue, ++uploadFenceValue);
		uploadFence.cpuWait();
	}

	// CPU mirrors of DistanceLattice.hlsli's q-space corner arithmetic --
	// used only by the tet-picking debug tool below, which needs to compute
	// tet corners itself now (no Tets buffer, no GPU round-trip needed at
	// all for this anymore -- see DistanceLattice.hlsli's header comment).
	// Kept in exact lockstep with the shader-side functions of the same
	// name/shape.
	static constexpr int CubeVertexOffsetsCpu[6][2][3] = {
		{ {1,0,0}, {1,0,1} },
		{ {1,0,0}, {1,1,0} },
		{ {0,1,0}, {1,1,0} },
		{ {0,1,0}, {0,1,1} },
		{ {0,0,1}, {0,1,1} },
		{ {0,0,1}, {1,0,1} },
	};

	// Not static -- CubeBoundDim/CubeOriginMin/GridRes/BDim/ACount are runtime
	// members now (grid resolution is a GUI slider), not compile-time
	// constants.
	void CpuCubeOriginFromLinear(uint lin, int& cx, int& cy, int& cz) {
		uint cd = CubeBoundDim;
		uint z = lin / (cd * cd);
		uint rem = lin % (cd * cd);
		uint y = rem / cd;
		uint x = rem % cd;
		cx = (int)x + CubeOriginMin;
		cy = (int)y + CubeOriginMin;
		cz = (int)z + CubeOriginMin;
	}

	// Mirrors DistanceLattice.hlsli's GetTetCornerQs.
	void CpuGetTetCornerQs(uint tetIndex, int q[4][3]) {
		uint cubeLin = tetIndex / 6;
		uint slot = tetIndex % 6;
		int cx, cy, cz;
		CpuCubeOriginFromLinear(cubeLin, cx, cy, cz);
		q[0][0] = cx;     q[0][1] = cy;     q[0][2] = cz;     // D0
		q[1][0] = cx + 1; q[1][1] = cy + 1; q[1][2] = cz + 1; // D1
		q[2][0] = cx + CubeVertexOffsetsCpu[slot][0][0];
		q[2][1] = cy + CubeVertexOffsetsCpu[slot][0][1];
		q[2][2] = cz + CubeVertexOffsetsCpu[slot][0][2];      // U0
		q[3][0] = cx + CubeVertexOffsetsCpu[slot][1][0];
		q[3][1] = cy + CubeVertexOffsetsCpu[slot][1][1];
		q[3][2] = cz + CubeVertexOffsetsCpu[slot][1][2];      // U1
	}

	// Mirrors DistanceLattice.hlsli's ResolveCorner -- returns false (nodeIdx
	// left at SENTINEL) for a virtual/out-of-grid corner.
	bool CpuResolveCorner(int qx, int qy, int qz, uint& nodeIdx) {
		bool isB = ((qx + qy + qz) & 1) != 0;
		int px = qx, py = qy, pz = qz;
		if (isB) { px -= 1; py -= 1; pz -= 1; }
		int i = (px + py - pz) / 2;
		int j = (px - py + pz) / 2;
		int k = (-px + py + pz) / 2;
		if (!isB) {
			if (i < 0 || j < 0 || k < 0 || i >= (int)GridRes || j >= (int)GridRes || k >= (int)GridRes) { nodeIdx = 0xFFFFFFFFu; return false; }
			nodeIdx = (uint)i + (uint)j * GridRes + (uint)k * GridRes * GridRes;
		} else {
			if (i < 0 || j < 0 || k < 0 || i >= (int)BDim || j >= (int)BDim || k >= (int)BDim) { nodeIdx = 0xFFFFFFFFu; return false; }
			nodeIdx = ACount + (uint)i + (uint)j * BDim + (uint)k * BDim * BDim;
		}
		return true;
	}

	// Mirrors DistanceLattice.hlsli's CubeLinearIndex.
	bool CpuCubeLinearIndex(int cx, int cy, int cz, uint& idx) {
		int sx = cx - CubeOriginMin, sy = cy - CubeOriginMin, sz = cz - CubeOriginMin;
		if (sx < 0 || sy < 0 || sz < 0 || sx >= (int)CubeBoundDim || sy >= (int)CubeBoundDim || sz >= (int)CubeBoundDim) return false;
		idx = (uint)sx + (uint)sy * CubeBoundDim + (uint)sz * CubeBoundDim * CubeBoundDim;
		return true;
	}

	// Mirrors extractSurfaceCS.hlsl's IsWithinRealWindow.
	bool CpuIsWithinRealWindow(int cx, int cy, int cz) {
		int i = (cx + cy - cz) / 2;
		int j = (cx - cy + cz) / 2;
		int k = (-cx + cy + cz) / 2;
		int center = (int)(GridRes / 2);
		int half = (int)WindowRealHalfExtent;
		return abs(i - center) <= half && abs(j - center) <= half && abs(k - center) <= half;
	}

	// Mirrors extractSurfaceCS.hlsl's GlobalTetIndexFromWindowLocal -- the
	// pick tool only ever needs to search the rendered window (nothing
	// outside it is ever visible to click on), not the whole domain. Also
	// applies the same real-space cutoff (CpuIsWithinRealWindow), so picking
	// never finds a tet that extractSurfaceCS.hlsl would have degenerated.
	bool CpuGlobalTetIndexFromWindowLocal(uint localT, uint& globalT) {
		uint localCubeLin = localT / 6;
		uint slot = localT % 6;
		uint wd = WindowCubeDim;
		uint lz = localCubeLin / (wd * wd);
		uint rem = localCubeLin % (wd * wd);
		uint ly = rem / wd;
		uint lx = rem % wd;
		int gx = (int)lx + windowOriginCubeX;
		int gy = (int)ly + windowOriginCubeY;
		int gz = (int)lz + windowOriginCubeZ;
		if (!CpuIsWithinRealWindow(gx, gy, gz)) { globalT = 0; return false; }
		uint globalLin;
		if (!CpuCubeLinearIndex(gx, gy, gz, globalLin)) { globalT = 0; return false; }
		globalT = globalLin * 6 + slot;
		return true;
	}

	// Mirrors DistanceLattice.hlsli's QWorldPos -- always succeeds, no range
	// check (extrapolates past the real grid for a virtual corner).
	static Egg::Math::float3 CpuQWorldPos(int qx, int qy, int qz) {
		using namespace Egg::Math;
		bool isB = ((qx + qy + qz) & 1) != 0;
		int px = qx, py = qy, pz = qz;
		if (isB) { px -= 1; py -= 1; pz -= 1; }
		float3 base((float)(px + py - pz) * 0.5f, (float)(px - py + pz) * 0.5f, (float)(-px + py + pz) * 0.5f);
		return (isB ? (base + float3(0.5f, 0.5f, 0.5f)) : base) * CellSize;
	}

	// Barycentric coordinates of p w.r.t. tet (P0,P1,P2,P3) -- same dual-
	// basis/reciprocal-vector construction as TetShapeGradients in
	// DistanceLattice.hlsli, just evaluated once for a point instead of
	// building shape-function gradients. Returns (w0,w1,w2,w3); p is inside
	// the tet (within tolerance) iff all 4 are >= -eps and sum to ~1.
	static Egg::Math::float4 BarycentricOf(const Egg::Math::float3& p,
		const Egg::Math::float3& P0, const Egg::Math::float3& P1,
		const Egg::Math::float3& P2, const Egg::Math::float3& P3) {
		using namespace Egg::Math;
		float3 e1 = P1 - P0, e2 = P2 - P0, e3 = P3 - P0;
		float3 d = p - P0;
		float3 c23 = e2.Cross(e3), c31 = e3.Cross(e1), c12 = e1.Cross(e2);
		float V = e1.Dot(c23);
		float invV = (fabsf(V) > 1.0e-8f) ? (1.0f / V) : 0.0f;
		float a = d.Dot(c23) * invV;
		float b = d.Dot(c31) * invV;
		float c = d.Dot(c12) * invV;
		return float4(1.0f - a - b - c, a, b, c);
	}

	void UploadPickedTetCb() {
		using namespace Egg::Math;
		if (pickedTetValid) {
			pickedTetCb.data.Valid = 1;
		} else {
			pickedTetCb.data.Valid = 0;
		}
		pickedTetCb.Upload();
	}

	// Brute-force point-in-tet search: pure CPU arithmetic now, no GPU
	// buffer/round-trip at all -- tet connectivity has no buffer to read
	// (see DistanceLattice.hlsli), every tet's corners are computed
	// directly from its index. Still a rare, user-triggered debug
	// operation, and now scoped to just the WINDOW (WindowTetCount, via
	// CpuGlobalTetIndexFromWindowLocal) rather than the whole domain --
	// nothing outside the rendered window is ever visible to click on, and
	// at large GridRes the whole domain is far too big to brute-force scan
	// interactively. Returns a GLOBAL tetIndex (needed by CpuGetTetCornerQs
	// to recover the tet's 4 corner nodes for the Picked Tet panel).
	// Skips slots with no real corner at all (a fully virtual/background
	// tet, nothing meaningful to pick there).
	uint FindEnclosingTet(const Egg::Math::float3& point) {
		using namespace Egg::Math;

		uint found = 0xFFFFFFFFu;
		const float eps = 0.02f;
		for (uint localT = 0; localT < WindowTetCount; localT++) {
			uint globalT;
			if (!CpuGlobalTetIndexFromWindowLocal(localT, globalT)) continue;

			int q[4][3];
			CpuGetTetCornerQs(globalT, q);

			float3 P[4];
			uint refs[4];
			bool anyReal = false;
			for (int c = 0; c < 4; c++) {
				P[c] = CpuQWorldPos(q[c][0], q[c][1], q[c][2]);
				if (CpuResolveCorner(q[c][0], q[c][1], q[c][2], refs[c])) anyReal = true;
			}
			if (!anyReal) continue;

			float4 bary = BarycentricOf(point, P[0], P[1], P[2], P[3]);
			if (bary.x >= -eps && bary.y >= -eps && bary.z >= -eps && bary.w >= -eps) {
				found = globalT;
				pickedTetCb.data.Corner0 = float4(P[0], 1.0f);
				pickedTetCb.data.Corner1 = float4(P[1], 1.0f);
				pickedTetCb.data.Corner2 = float4(P[2], 1.0f);
				pickedTetCb.data.Corner3 = float4(P[3], 1.0f);
				pickedTetNodes[0] = refs[0]; pickedTetNodes[1] = refs[1]; pickedTetNodes[2] = refs[2]; pickedTetNodes[3] = refs[3];
				break;
			}
		}
		return found;
	}

	// Debug tool: reconstruct the world-space point the mouse is currently
	// over (by reading back the single depth texel at the cursor from
	// depthStencilBuffer -- Egg::SimpleApp's own D32_FLOAT depth buffer --
	// and unprojecting via the inverse view-projection matrix), then
	// brute-force-search for which tet contains that point, and mark it for
	// the wireframe pass to draw. Triggered on demand (GUI button), never
	// per-frame -- the depth readback needs a GPU round-trip/CPU wait.
	//
	// Caveat: since the depth buffer may also contain the raymarch
	// reference shape's depth (drawn first, ALWAYS+write) wherever the
	// extracted surface fails its own depth test against it (i.e. the
	// solved surface lags slightly behind the analytic reference at that
	// pixel), picking is most accurate with "Show Reference Shape" off.
	void PerformPick() {
		using namespace Egg::Math;

		pickedTetValid = false;

		if (!dataValid || mouseX < 0 || mouseY < 0 ||
			mouseX >= (int)viewPort.Width || mouseY >= (int)viewPort.Height) {
			UploadPickedTetCb();
			return;
		}

		const UINT readbackRowPitch = 256; // D3D12_TEXTURE_DATA_PITCH_ALIGNMENT
		com_ptr<ID3D12Resource> readback = CreateReadbackBuffer(device.Get(), readbackRowPitch);

		DX_API("reset upload allocator (pick depth)") uploadAllocator->Reset();
		DX_API("reset upload command list (pick depth)") uploadCommandList->Reset(uploadAllocator.Get(), nullptr);
		auto& cmd = uploadCommandList;

		cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			depthStencilBuffer.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_COPY_SOURCE));

		D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
		srcLoc.pResource = depthStencilBuffer.Get();
		srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		srcLoc.SubresourceIndex = 0;

		D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
		dstLoc.pResource = readback.Get();
		dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		dstLoc.PlacedFootprint.Offset = 0;
		dstLoc.PlacedFootprint.Footprint.Format = DXGI_FORMAT_D32_FLOAT;
		dstLoc.PlacedFootprint.Footprint.Width = 1;
		dstLoc.PlacedFootprint.Footprint.Height = 1;
		dstLoc.PlacedFootprint.Footprint.Depth = 1;
		dstLoc.PlacedFootprint.Footprint.RowPitch = readbackRowPitch;

		D3D12_BOX srcBox = { (UINT)mouseX, (UINT)mouseY, 0, (UINT)mouseX + 1, (UINT)mouseY + 1, 1 };
		cmd->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, &srcBox);

		cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			depthStencilBuffer.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE));

		DX_API("close upload command list (pick depth)") cmd->Close();
		{
			ID3D12CommandList* lists[] = { cmd.Get() };
			commandQueue->ExecuteCommandLists(1, lists);
		}
		uploadFence.signal(commandQueue, ++uploadFenceValue);
		uploadFence.cpuWait();

		float depth = 1.0f;
		float* mapped = nullptr;
		D3D12_RANGE readRange = { 0, sizeof(float) };
		readback->Map(0, &readRange, (void**)&mapped);
		depth = mapped[0];
		D3D12_RANGE writtenRange = { 0, 0 };
		readback->Unmap(0, &writtenRange);

		if (depth >= 0.99999f) {
			UploadPickedTetCb(); // missed everything -- background/far clear
			return;
		}

		float ndcX = ((float)mouseX + 0.5f) / viewPort.Width * 2.0f - 1.0f;
		float ndcY = 1.0f - ((float)mouseY + 0.5f) / viewPort.Height * 2.0f;

		float4x4 viewProj = camera->GetViewMatrix() * camera->GetProjMatrix();
		float4x4 invViewProj = viewProj.Invert();
		float4 clipPos(ndcX, ndcY, depth, 1.0f);
		float4 worldPos4 = clipPos * invViewProj;
		float3 worldPos = float3(worldPos4.x, worldPos4.y, worldPos4.z) / worldPos4.w;

		uint foundTet = FindEnclosingTet(worldPos);
		pickedTetValid = (foundTet != 0xFFFFFFFFu);
		pickedTetIndex = foundTet;
		if (pickedTetValid) ReadBackPickedTetDiagnostics();
		else pickedDiagnosticsValid = false;
		UploadPickedTetCb();
	}

	// Reads back the full 6-slot candidate/potential arrays and the volume-
	// floor fields (CurrentVolume, IsConnecting) for the picked tet's 4
	// corners, for the "Picked Tet" GUI panel -- lets you directly inspect
	// the actual solved numbers at a specific wedge inset/outcrop instead of
	// guessing from the picture. No TetInterfacePair buffer anymore --
	// pickedInterfaceLabelI/J are derived the same way extractSurfaceCS.hlsl
	// now derives a tet's dominant pair: frequency-vote over the 4 corners'
	// own top (argmax) candidate, independently, right here on the CPU.
	void ReadBackPickedTetDiagnostics() {
		UINT64 candBytes = (UINT64)NodeCount * 2 * sizeof(UINT);
		UINT64 potBytes = (UINT64)NodeCount * MAX_CANDIDATES * sizeof(float);
		UINT64 nodeFloatBytes = (UINT64)NodeCount * sizeof(float);
		UINT64 connectingBytes = (UINT64)ACount * sizeof(UINT);
		UINT64 footDistBytes = (UINT64)ACount * sizeof(float);
		UINT64 footSeedBytes = (UINT64)ACount * sizeof(UINT);
		com_ptr<ID3D12Resource> rbCand = CreateReadbackBuffer(device.Get(), candBytes);
		com_ptr<ID3D12Resource> rbPot = CreateReadbackBuffer(device.Get(), potBytes);
		com_ptr<ID3D12Resource> rbCurVol = CreateReadbackBuffer(device.Get(), nodeFloatBytes);
		com_ptr<ID3D12Resource> rbConnecting = CreateReadbackBuffer(device.Get(), connectingBytes);
		com_ptr<ID3D12Resource> rbFootDist = CreateReadbackBuffer(device.Get(), footDistBytes);
		com_ptr<ID3D12Resource> rbFootSeed = CreateReadbackBuffer(device.Get(), footSeedBytes);

		DX_API("reset upload allocator (pick diag)") uploadAllocator->Reset();
		DX_API("reset upload command list (pick diag)") uploadCommandList->Reset(uploadAllocator.Get(), nullptr);
		auto& cmd = uploadCommandList;
		cmd->CopyBufferRegion(rbCand.Get(), 0, nodeCandidateLabelBuffer.Get(), 0, candBytes);
		cmd->CopyBufferRegion(rbPot.Get(), 0, nodePotentialBuffer.Get(), 0, potBytes);
		cmd->CopyBufferRegion(rbCurVol.Get(), 0, nodeCurrentVolumeBuffer.Get(), 0, nodeFloatBytes);
		cmd->CopyBufferRegion(rbConnecting.Get(), 0, nodeIsConnectingBuffer.Get(), 0, connectingBytes);
		cmd->CopyBufferRegion(rbFootDist.Get(), 0, nodeFootDistBuffer.Get(), 0, footDistBytes);
		cmd->CopyBufferRegion(rbFootSeed.Get(), 0, (jfaFinalIsBufferA ? jfaSeedBufferA : jfaSeedBufferB).Get(), 0, footSeedBytes);
		DX_API("close upload command list (pick diag)") cmd->Close();
		{
			ID3D12CommandList* lists[] = { cmd.Get() };
			commandQueue->ExecuteCommandLists(1, lists);
		}
		uploadFence.signal(commandQueue, ++uploadFenceValue);
		uploadFence.cpuWait();

		UINT* cand = nullptr;
		float* pot = nullptr;
		float* curVol = nullptr;
		UINT* connecting = nullptr;
		float* footDist = nullptr;
		UINT* footSeed = nullptr;
		rbCand->Map(0, nullptr, (void**)&cand);
		rbPot->Map(0, nullptr, (void**)&pot);
		rbCurVol->Map(0, nullptr, (void**)&curVol);
		rbConnecting->Map(0, nullptr, (void**)&connecting);
		rbFootDist->Map(0, nullptr, (void**)&footDist);
		rbFootSeed->Map(0, nullptr, (void**)&footSeed);

		for (uint c = 0; c < 4; c++) {
			uint node = pickedTetNodes[c];
			if (node == 0xFFFFFFFFu) {
				// Virtual corner (outside the real grid) -- mirrors
				// GetCornerTopLabel/GetCornerPotential's convention: label 0
				// with potential 1.0, everything else absent, no real
				// volume/connecting data to show.
				pickedCornerLabels[c][0] = 0;
				pickedCornerPots[c][0] = 1.0f;
				for (uint s = 1; s < MAX_CANDIDATES; s++) {
					pickedCornerLabels[c][s] = SENTINEL_CANDIDATE;
					pickedCornerPots[c][s] = 0.0f;
				}
				pickedCornerCurrentVolume[c] = 0.0f;
				pickedCornerIsConnecting[c] = 0;
				pickedCornerFootDist[c] = -1.0f;
				pickedCornerFootSeed[c] = SENTINEL_LABEL;
				continue;
			}
			for (uint s = 0; s < MAX_CANDIDATES; s++) {
				pickedCornerLabels[c][s] = (cand[node * 2u + s / 4u] >> ((s % 4u) * 8u)) & 0xFFu;
				pickedCornerPots[c][s] = pot[node * MAX_CANDIDATES + s];
			}
			pickedCornerCurrentVolume[c] = curVol[node];
			pickedCornerIsConnecting[c] = (node < ACount) ? connecting[node] : 0;
			pickedCornerFootDist[c] = (node < ACount) ? footDist[node] : -1.0f;
			pickedCornerFootSeed[c] = (node < ACount) ? footSeed[node] : SENTINEL_LABEL;
		}

		// Independent per-tet dominant pair -- same frequency-vote rule as
		// extractSurfaceCS.hlsl, over this tet's own 4 corners' top label.
		{
			uint cornerTop[4];
			for (uint c = 0; c < 4; c++) {
				uint bestLabel = SENTINEL_CANDIDATE; float bestPot = -1.0e30f;
				for (uint s = 0; s < MAX_CANDIDATES; s++) {
					uint l = pickedCornerLabels[c][s];
					if (l == SENTINEL_CANDIDATE) continue;
					float p = pickedCornerPots[c][s];
					if (p > bestPot) { bestPot = p; bestLabel = l; }
				}
				cornerTop[c] = bestLabel;
			}
			uint uniqueLabels[4] = { SENTINEL_CANDIDATE, SENTINEL_CANDIDATE, SENTINEL_CANDIDATE, SENTINEL_CANDIDATE };
			int freq[4] = { 0, 0, 0, 0 };
			uint nUnique = 0;
			for (uint c = 0; c < 4; c++) {
				uint l = cornerTop[c];
				bool found = false;
				for (uint u = 0; u < nUnique; u++) if (uniqueLabels[u] == l) { freq[u]++; found = true; break; }
				if (!found) { uniqueLabels[nUnique] = l; freq[nUnique] = 1; nUnique++; }
			}
			for (uint p = 0; p < 4; p++) {
				for (uint s = 0; s < 3; s++) {
					bool doSwap = (freq[s] < freq[s + 1]) || (freq[s] == freq[s + 1] && uniqueLabels[s] > uniqueLabels[s + 1]);
					if (doSwap) {
						int tf = freq[s]; freq[s] = freq[s + 1]; freq[s + 1] = tf;
						uint tl = uniqueLabels[s]; uniqueLabels[s] = uniqueLabels[s + 1]; uniqueLabels[s + 1] = tl;
					}
				}
			}
			pickedInterfaceLabelI = uniqueLabels[0];
			pickedInterfaceLabelJ = (freq[1] > 0) ? uniqueLabels[1] : pickedInterfaceLabelI;
			if (pickedInterfaceLabelJ < pickedInterfaceLabelI) { uint tmp = pickedInterfaceLabelI; pickedInterfaceLabelI = pickedInterfaceLabelJ; pickedInterfaceLabelJ = tmp; }
		}
		pickedDiagnosticsValid = true;

		rbCand->Unmap(0, nullptr);
		rbPot->Unmap(0, nullptr);
		rbCurVol->Unmap(0, nullptr);
		rbConnecting->Unmap(0, nullptr);
	}

	void BuildImGui() {
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::SetNextWindowSize(ImVec2(280, 0), ImGuiCond_FirstUseEver);
		ImGui::Begin("g-Distance Controls");
		static const char* testShapeItems[] = {
			"3 Tori (multi-label)", "Ellipsoid", "Single Point", "Line", "Diagonal Line 2D", "Diagonal Line 3D", "Slab", "Single Torus (2-label)", "2x2x2 Box"
		};
		ImGui::Combo("Test Shape", &testShapeKind, testShapeItems, IM_ARRAYSIZE(testShapeItems));
		ImGui::SliderInt("Grid Resolution", &gridResSetting, 4, 256, "%d", ImGuiSliderFlags_Logarithmic);
		ImGui::SliderInt("Render Window Size", &windowCubeDimSetting, 8, 48);
		ImGui::TextDisabled("(real grid units, GridRes-independent -- only this much of the domain, centered, is ever extracted/rendered)");
		{
			// ~0.125 KB * GridRes^3 for the per-node arrays (scale with the
			// whole solved domain, not just the render window -- see the
			// approved plan's memory-scaling discussion). tetInterfacePairBuffer
			// is gone entirely (assignInterfacePairsCS.hlsl removed along with
			// the cube-vote combinatorics -- see the edge-centric design
			// discussion), which is why this dropped from ~504 to ~128: that
			// buffer alone was 384 of the old 504 KB/GridRes^3 (the remaining
			// +8 vs. an earlier ~120 estimate is nodeFrozenWinnerBuffer, added
			// to restore the old "freeze combinatorics for one round" cadence
			// -- see snapshotWinnerCS.hlsl).
			// surfaceVertexBuffer's actual q-space dispatch size is
			// 2*windowCubeDimSetting+8 (clamped to the domain's own bounding
			// box) -- see EnsureGridBuffersSized's comment on why a q-space
			// box must be oversized to safely contain the intended real
			// window under the bccToRhombo shear.
			double gr = (double)gridResSetting;
			double cubeBoundDimEst = 2.0 * gr;
			double qDispatch = 2.0 * (double)windowCubeDimSetting + 8.0;
			if (qDispatch > cubeBoundDimEst) qDispatch = cubeBoundDimEst;
			double solveBytes = 128.0 * gr * gr * gr;
			double windowBytes = qDispatch * qDispatch * qDispatch * 6.0 * 192.0;
			double totalMB = (solveBytes + windowBytes) / (1024.0 * 1024.0);
			ImGui::TextDisabled("Estimated buffer memory at these settings: ~%.1f MB", totalMB);
		}
		ImGui::SliderInt("Iterations", &iterations, 0, 64);
		ImGui::SliderFloat("Seed Jitter", &seedJitter, 0.0f, 1.0f);
		ImGui::SliderFloat("Own Label Seed", &ownLabelSeed, 0.0f, 5.0f);
		ImGui::Checkbox("Neutral B Seed (+jitter)", &neutralBSeed);
		ImGui::TextDisabled("(off = old majority-vote B seed)");
		ImGui::Checkbox("Edge Connectivity Only", &edgeConnectivityOnly);
		ImGui::TextDisabled("(18- vs 26-connectivity for pinning connecting nodes)");
		ImGui::TextDisabled("(all of the above applied on Reinitialize)");

		if (dataValid) {
			ImGui::SliderInt("Continue Iterations", &continueIterations, 1, 64);
			if (ImGui::Button("Continue")) needsContinue = true;
			ImGui::SameLine();
			ImGui::TextDisabled("('C' key)");
		}
		if (ImGui::Button("Reinitialize")) needsReinit = true;
		ImGui::SameLine();
		ImGui::TextDisabled("('R' key)");

		ImGui::SliderInt("Jacobi Sweeps / Round", &jacobiSweepsPerRound, 1, 32);

		ImGui::Checkbox("Show Nodes", &showNodes);
		ImGui::SameLine();
		ImGui::Checkbox("Show Surface", &showSurface);
		ImGui::Checkbox("Show Reference Shape", &showReference);
		ImGui::Checkbox("Hide Uniform-Neighborhood Nodes", &hideUniformNodes);

		ImGui::Checkbox("Show Potential Slice", &showPotentialSlice);
		ImGui::SameLine();
		ImGui::TextDisabled("(debug: cross-section, red=label 0 green=label 1)");
		if (showPotentialSlice) {
			static const char* axisItems[] = { "X", "Y", "Z" };
			ImGui::Combo("Slice Axis", &sliceAxis, axisItems, 3);
			ImGui::SliderInt("Slice Index", &sliceIndex, 0, (int)GridRes - 1);
			ImGui::SliderFloat("Potential Color Scale", &potentialColorScale, 0.05f, 5.0f);
		}

		if (dataValid) {
			if (pickedTetValid) ImGui::Text("Picked tet (right-click): %u", pickedTetIndex);
			else ImGui::TextDisabled("Picked tet (right-click): none (or last click missed)");
			ImGui::TextDisabled("(reads the depth buffer -- disable 'Show Reference Shape' for accuracy)");

			if (pickedTetValid && pickedDiagnosticsValid) {
				static const char* cornerNames[4] = { "A0", "A1", "B0", "B1" };
				float phiI[4], phiJ[4];
				bool hasI[4], hasJ[4];
				for (uint c = 0; c < 4; c++) {
					phiI[c] = 0.0f; phiJ[c] = 0.0f; hasI[c] = false; hasJ[c] = false;
					for (uint s = 0; s < MAX_CANDIDATES; s++) {
						uint l = pickedCornerLabels[c][s];
						if (l == pickedInterfaceLabelI) { phiI[c] = pickedCornerPots[c][s]; hasI[c] = true; }
						if (l == pickedInterfaceLabelJ) { phiJ[c] = pickedCornerPots[c][s]; hasJ[c] = true; }
					}
				}

				ImGui::Text("Active pair: label %u vs label %u", pickedInterfaceLabelI, pickedInterfaceLabelJ);
				for (uint c = 0; c < 4; c++) {
					uint node = pickedTetNodes[c];
					ImGui::Text("  %s (node %u, %s): phi_i=%s%.4f  phi_j=%s%.4f  g=%.4f",
						cornerNames[c], node, (node < ACount) ? "A" : "B",
						hasI[c] ? "" : "*", phiI[c],
						hasJ[c] ? "" : "*", phiJ[c],
						phiI[c] - phiJ[c]);
					ImGui::Text("      CurVol=%.4f%s%s",
						pickedCornerCurrentVolume[c],
						(pickedCornerIsConnecting[c] & 1u) ? "  CONNECTING (floor pinned)" : "",
						(pickedCornerIsConnecting[c] & 2u) ? "  LOCAL-MAX(footdist)" : "");
					if (pickedCornerFootDist[c] >= 0.0f) {
						uint node = pickedTetNodes[c];
						uint seed = pickedCornerFootSeed[c];
						if (seed == SENTINEL_LABEL)
							ImGui::Text("      NodeFootDist(raw)=%.6f  seed=SENTINEL (no boundary found!)", pickedCornerFootDist[c]);
						else if (seed == node)
							ImGui::Text("      NodeFootDist(raw)=%.6f  seed=SELF (node %u) -- BUG", pickedCornerFootDist[c], seed);
						else
							ImGui::Text("      NodeFootDist(raw)=%.6f  seed=node %u", pickedCornerFootDist[c], seed);
					} else {
						ImGui::TextDisabled("      NodeFootDist(raw)=N/A (B-node)");
					}
				}
				ImGui::TextDisabled("(* = label not a candidate here, falls back to 0)");

				if (ImGui::Button("Copy Picked Tet Info")) {
					char line[384];
					std::string report;
					sprintf_s(line, sizeof(line), "Picked tet %u\r\nActive pair: label %u vs label %u\r\n",
						pickedTetIndex, pickedInterfaceLabelI, pickedInterfaceLabelJ);
					report += line;
					for (uint c = 0; c < 4; c++) {
						uint node = pickedTetNodes[c];
						char footDistStr[64];
						if (pickedCornerFootDist[c] >= 0.0f) {
							uint seed = pickedCornerFootSeed[c];
							if (seed == SENTINEL_LABEL)
								sprintf_s(footDistStr, sizeof(footDistStr), "%.6f (seed=SENTINEL)", pickedCornerFootDist[c]);
							else if (seed == node)
								sprintf_s(footDistStr, sizeof(footDistStr), "%.6f (seed=SELF -- BUG)", pickedCornerFootDist[c]);
							else
								sprintf_s(footDistStr, sizeof(footDistStr), "%.6f (seed=node %u)", pickedCornerFootDist[c], seed);
						} else {
							sprintf_s(footDistStr, sizeof(footDistStr), "N/A (B-node)");
						}
						sprintf_s(line, sizeof(line), "%s (node %u, %s): phi_i=%s%.6f  phi_j=%s%.6f  g=%.6f\r\n"
							"    CurVol=%.6f  IsConnecting=%u  NodeFootDist(raw)=%s\r\n",
							cornerNames[c], node, (node < ACount) ? "A" : "B",
							hasI[c] ? "" : "*", phiI[c],
							hasJ[c] ? "" : "*", phiJ[c],
							phiI[c] - phiJ[c],
							pickedCornerCurrentVolume[c], pickedCornerIsConnecting[c],
							footDistStr);
						report += line;
					}
					ImGui::SetClipboardText(report.c_str());
				}
			}
		}

		if (ImGui::CollapsingHeader("Energy Weights")) {
			ImGui::Checkbox("Use Block Smoothing (experimental)", &useBlockSmoothing);
			ImGui::SameLine();
			ImGui::TextDisabled("(Term 1 ONLY, tile/groupshared, see smoothnessJacobiBlockCS.hlsl -- Terms 2-5 off while enabled)");
			ImGui::Checkbox("Term 1: Edge-Walk Traversal", &useEdgeWalkTraversal);
			ImGui::SameLine();
			ImGui::TextDisabled("(off = node-adjacent-tets, see smoothnessJacobiCS.hlsl; ignored by Block Smoothing)");
			ImGui::SliderFloat("Smoothness Weight", &smoothnessWeight, 0.0f, 10.0f);
			ImGui::SliderFloat("Margin Weight", &marginWeight, 0.0f, 10.0f);
			ImGui::SliderFloat("Margin Target", &marginTarget, 0.0f, 2.0f);
			ImGui::SliderFloat("Regularizer Weight", &regularizerWeight, 0.0f, 1.0f);
			ImGui::SliderFloat("Jacobi Diag Epsilon", &jacobiDiagEpsilon, 0.001f, 1.0f);
			ImGui::SliderFloat("Max Potential Step", &maxPotentialStep, 0.0001f, 0.1f, "%.4f");
			// Range is huge (not 0..5 like the other weights) because Term 4's
			// natural gradient magnitude is orders of magnitude smaller than
			// smoothness's (TetShapeGradients on these thin BCC disphenoids
			// produces large shape-gradient differences, while the volume
			// term's K=VSide/sumSide stays O(0.1-1)) -- confirmed via direct
			// GPU-readback probe (kSum0/diag0) that ~800-5000 is the range
			// where it actually starts overpowering smoothness's perpetual,
			// never-converging push at a topologically point- or line-like
			// feature (which can never be locally smooth anywhere along it,
			// so smoothness never settles there and keeps eroding it every
			// sweep, forever, regardless of round count) -- only applies to
			// nodes flagged connecting (see "Edge Connectivity Only" above),
			// and only as a one-sided floor: no penalty at all once a
			// connecting node's own volume is at or above Volume Floor.
			ImGui::SliderFloat("Volume Weight", &volumeWeight, 0.0f, 10000.0f, "%.1f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderFloat("Volume Floor", &volumeFloor, 0.0f, 2.0f);
			ImGui::SliderFloat("Missing Candidate Fallback", &missingFallback, -2.0f, 1.0f);
			ImGui::SameLine();
			ImGui::TextDisabled("(TetFieldGrad's GetCornerPotential fallback, Term 1)");
			ImGui::SliderFloat("Distance Term Weight", &distanceWeight, 0.0f, 10.0f);
			ImGui::SameLine();
			ImGui::TextDisabled("(term 5: same-label edge diff ~= edge length, see smoothnessJacobiCS.hlsl)");
		}

		ImGui::SliderFloat("Point Radius", &pointRadiusPx, 0.5f, 10.0f);
		ImGui::SliderFloat("Potential Size Scale", &potentialSizeScale, 0.05f, 5.0f);
		ImGui::SliderFloat("Node Depth Fade Exponent", &nodeFadeExponent, 0.0f, 10.0f);
		ImGui::SliderFloat("Node Fade Start Distance", &nodeFadeStartDistance, 0.0f, 3.0f);

		ImGui::Separator();
		ImGui::Text("A nodes: %u   B nodes: %u   Tets (domain/window): %u / %u", ACount, BCount, TetCount, WindowTetCount);
		ImGui::Text("%.1f FPS", ImGui::GetIO().Framerate);
		ImGui::End();

		ImGui::Render();
		ID3D12DescriptorHeap* imguiHeaps[] = { imguiSrvHeap.Get() };
		commandList->SetDescriptorHeaps(1, imguiHeaps);
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.Get());
	}

public:
	DistanceApp() : SimpleApp() {}

	void InitImGui(HWND hwnd) {
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();

		ImGui_ImplWin32_Init(hwnd);

		uint dhIncrSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		ImGui_ImplDX12_InitInfo initInfo;
		initInfo.Device = device.Get();
		initInfo.CommandQueue = commandQueue.Get();
		initInfo.NumFramesInFlight = (int)swapChainBackBufferCount;
		initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		initInfo.SrvDescriptorHeap = imguiSrvHeap.Get();
		initInfo.LegacySingleSrvCpuDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(imguiSrvHeap->GetCPUDescriptorHandleForHeapStart(), 0, dhIncrSize);
		initInfo.LegacySingleSrvGpuDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(imguiSrvHeap->GetGPUDescriptorHandleForHeapStart(), 0, dhIncrSize);
		ImGui_ImplDX12_Init(&initInfo);
	}

	void ShutdownImGui() {
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}

	virtual void CreateResources() override {
		Egg::SimpleApp::CreateResources();

		frameCount = 0;
		uploadFence.createResources(device);

		frameCb.CreateResources(device.Get());
		distanceCb.CreateResources(device.Get());
		distanceGridCb.CreateResources(device.Get());
		torusCb.CreateResources(device.Get());
		pickedTetCb.CreateResources(device.Get());

		D3D12_DESCRIPTOR_HEAP_DESC imguiHeapDesc = {};
		imguiHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		imguiHeapDesc.NumDescriptors = 1;
		imguiHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		DX_API("create imgui srv heap")
			device->CreateDescriptorHeap(&imguiHeapDesc, IID_PPV_ARGS(imguiSrvHeap.GetAddressOf()));

		// rasterLabelBuffer/nodeIsConnectingBuffer/nodeCandidateLabelBuffer/
		// nodePotentialBuffer(+scratch)/surfaceVertexBuffer/
		// nodeCurrentVolumeBuffer are all sized by the runtime grid
		// resolution -- created (and resized on later grid-resolution
		// changes) by EnsureGridBuffersSized(), called from the first
		// RunReinit() rather than here.

		rasterLabelCS.createResources(device, "Shaders/rasterLabelCS.cso");
		computeConnectingNodesCS.createResources(device, "Shaders/computeConnectingNodesCS.cso");
		jfaInitCS.createResources(device, "Shaders/jfaInitCS.cso");
		jfaStepCS.createResources(device, "Shaders/jfaStepCS.cso");
		jfaFinalizeCS.createResources(device, "Shaders/jfaFinalizeCS.cso");
		buildCandidatesCS.createResources(device, "Shaders/buildCandidatesCS.cso");
		snapshotWinnerCS.createResources(device, "Shaders/snapshotWinnerCS.cso");
		smoothnessJacobiCS.createResources(device, "Shaders/smoothnessJacobiCS.cso");
		smoothnessJacobiBlockCS.createResources(device, "Shaders/smoothnessJacobiBlockCS.cso");
		commitPotentialCS.createResources(device, "Shaders/commitPotentialCS.cso");
		extractSurfaceCS.createResources(device, "Shaders/extractSurfaceCS.cso");

		{
			com_ptr<ID3DBlob> vs = Egg::Shader::LoadCso("Shaders/raymarchVS.cso");
			com_ptr<ID3DBlob> ps = Egg::Shader::LoadCso("Shaders/raymarchPS.cso");
			raymarchRootSig = Egg::Shader::LoadRootSignature(device.Get(), vs.Get());

			D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
			psoDesc.pRootSignature = raymarchRootSig.Get();
			psoDesc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
			psoDesc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
			psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
			psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			psoDesc.DepthStencilState.DepthEnable = TRUE;
			psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
			psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			D3D12_INPUT_LAYOUT_DESC emptyLayout = { nullptr, 0 };
			psoDesc.InputLayout = emptyLayout;
			psoDesc.NumRenderTargets = 1;
			psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
			psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
			psoDesc.SampleMask = UINT_MAX;
			psoDesc.SampleDesc.Count = 1;
			DX_API("create raymarch PSO")
				device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(raymarchPso.GetAddressOf()));
		}

		{
			com_ptr<ID3DBlob> vs = Egg::Shader::LoadCso("Shaders/nodePointVS.cso");
			com_ptr<ID3DBlob> ps = Egg::Shader::LoadCso("Shaders/nodePointPS.cso");
			nodePointRootSig = Egg::Shader::LoadRootSignature(device.Get(), vs.Get());

			D3D12_BLEND_DESC alphaBlend = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			alphaBlend.RenderTarget[0].BlendEnable    = TRUE;
			alphaBlend.RenderTarget[0].SrcBlend       = D3D12_BLEND_SRC_ALPHA;
			alphaBlend.RenderTarget[0].DestBlend      = D3D12_BLEND_INV_SRC_ALPHA;
			alphaBlend.RenderTarget[0].BlendOp        = D3D12_BLEND_OP_ADD;
			alphaBlend.RenderTarget[0].SrcBlendAlpha  = D3D12_BLEND_ONE;
			alphaBlend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
			alphaBlend.RenderTarget[0].BlendOpAlpha   = D3D12_BLEND_OP_ADD;

			D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
			psoDesc.pRootSignature = nodePointRootSig.Get();
			psoDesc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
			psoDesc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
			psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
			psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			psoDesc.DepthStencilState.DepthEnable = TRUE;
			// ALWAYS, not LESS: nodes sit almost exactly on the surface the
			// background raymarch already draws -- same z-fighting/far-side
			// depth-precision reasoning as g-Aequor's particlePointPso.
			psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
			psoDesc.BlendState = alphaBlend;
			D3D12_INPUT_LAYOUT_DESC emptyLayout = { nullptr, 0 };
			psoDesc.InputLayout = emptyLayout;
			psoDesc.NumRenderTargets = 1;
			psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
			psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
			psoDesc.SampleMask = UINT_MAX;
			psoDesc.SampleDesc.Count = 1;
			DX_API("create node point PSO")
				device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(nodePointPso.GetAddressOf()));
		}

		{
			com_ptr<ID3DBlob> vs = Egg::Shader::LoadCso("Shaders/surfaceVS.cso");
			com_ptr<ID3DBlob> ps = Egg::Shader::LoadCso("Shaders/surfacePS.cso");
			surfaceRootSig = Egg::Shader::LoadRootSignature(device.Get(), vs.Get());

			D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
			psoDesc.pRootSignature = surfaceRootSig.Get();
			psoDesc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
			psoDesc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
			psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
			// Real geometry (unlike the billboard/reference passes above), so
			// use standard LESS depth test + write -- it needs to both hide
			// behind the background where appropriate and self-occlude.
			psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			psoDesc.DepthStencilState.DepthEnable = TRUE;
			psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
			psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
			psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			D3D12_INPUT_LAYOUT_DESC emptyLayout = { nullptr, 0 };
			psoDesc.InputLayout = emptyLayout;
			psoDesc.NumRenderTargets = 1;
			psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
			psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
			psoDesc.SampleMask = UINT_MAX;
			psoDesc.SampleDesc.Count = 1;
			DX_API("create surface PSO")
				device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(surfacePso.GetAddressOf()));
		}

		{
			com_ptr<ID3DBlob> vs = Egg::Shader::LoadCso("Shaders/wireframeVS.cso");
			com_ptr<ID3DBlob> ps = Egg::Shader::LoadCso("Shaders/wireframePS.cso");
			wireframeRootSig = Egg::Shader::LoadRootSignature(device.Get(), vs.Get());

			D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
			psoDesc.pRootSignature = wireframeRootSig.Get();
			psoDesc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
			psoDesc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
			psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
			psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
			// ALWAYS + no write, same as nodes -- a debug marker should
			// always be visible on top, not fight the surface's own depth.
			psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			psoDesc.DepthStencilState.DepthEnable = TRUE;
			psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
			psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			D3D12_INPUT_LAYOUT_DESC emptyLayout = { nullptr, 0 };
			psoDesc.InputLayout = emptyLayout;
			psoDesc.NumRenderTargets = 1;
			psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
			psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
			psoDesc.SampleMask = UINT_MAX;
			psoDesc.SampleDesc.Count = 1;
			DX_API("create wireframe PSO")
				device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(wireframePso.GetAddressOf()));
		}

		{
			com_ptr<ID3DBlob> vs = Egg::Shader::LoadCso("Shaders/footSliceVS.cso");
			com_ptr<ID3DBlob> ps = Egg::Shader::LoadCso("Shaders/footSlicePS.cso");
			footSliceRootSig = Egg::Shader::LoadRootSignature(device.Get(), vs.Get());

			D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
			psoDesc.pRootSignature = footSliceRootSig.Get();
			psoDesc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
			psoDesc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
			psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
			// Real plane geometry (ray-plane intersection in the PS, written
			// to SV_Depth), same LESS/write convention as surfacePso -- it
			// should hide behind closer real geometry and be hidden by it.
			psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			psoDesc.DepthStencilState.DepthEnable = TRUE;
			psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
			psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
			psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			D3D12_INPUT_LAYOUT_DESC emptyLayout = { nullptr, 0 };
			psoDesc.InputLayout = emptyLayout;
			psoDesc.NumRenderTargets = 1;
			psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
			psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
			psoDesc.SampleMask = UINT_MAX;
			psoDesc.SampleDesc.Count = 1;
			DX_API("create foot-dist slice PSO")
				device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(footSlicePso.GetAddressOf()));
		}

		DX_API("create upload command allocator.")
			device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
				IID_PPV_ARGS(uploadAllocator.ReleaseAndGetAddressOf()));

		DX_API("create upload command list.")
			device->CreateCommandList(
				0, D3D12_COMMAND_LIST_TYPE_DIRECT,
				uploadAllocator.Get(), nullptr,
				IID_PPV_ARGS(uploadCommandList.ReleaseAndGetAddressOf()));

		DX_API("close upload command list.") uploadCommandList->Close();
		ID3D12CommandList* ppCommandLists[] = { uploadCommandList.Get() };
		commandQueue->ExecuteCommandLists(1, ppCommandLists);

		uploadFence.signal(commandQueue, ++uploadFenceValue);
		uploadFence.cpuWait();
	}

	virtual void CreateSwapChainResources() override {
		__super::CreateSwapChainResources();
		if (camera) camera->SetAspect(aspectRatio);
	}

	virtual void LoadAssets() override {
		using namespace Egg::Math;

		BuildShapeList();
		torusCb.Upload();

		camera = Egg::Cam::FirstPerson::Create();
		camera->SetProj(0.9f, aspectRatio, 0.1f, MaxMarchDist);
		camera->SetAspect(aspectRatio);
		camera->SetSpeed(0.15f * GridRes);
		float3 center(GridRes * CellSize * 0.5f, GridRes * CellSize * 0.5f, GridRes * CellSize * 0.5f);
		float3 eye = center + float3(0.0f, GridRes * 0.6f, GridRes * -1.0f);
		camera->SetView(eye, (center - eye).Normalize());

		RunReinit();
	}

	virtual void Render() override {
		if (needsReinit) {
			RunReinit();
			needsReinit = false;
		} else if (needsContinue) {
			RunContinue();
			needsContinue = false;
			if (pickedTetValid) ReadBackPickedTetDiagnostics(); // keep the Picked Tet panel current, not a stale snapshot from before this Continue
		}

		PopulateCommandList();

		ID3D12CommandList* cLists[] = { commandList.Get() };
		commandQueue->ExecuteCommandLists(1, cLists);

		DX_API("Failed to present swap chain")
			swapChain->Present(1, 0);

		WaitForPreviousFrame();
		frameCount++;

		if (pickRequested) {
			// Safe to read depthStencilBuffer now -- WaitForPreviousFrame
			// just confirmed the GPU finished this frame's PopulateCommandList
			// (including whatever it wrote to the depth buffer).
			PerformPick();
			pickRequested = false;
		}
	}

	virtual void PopulateCommandList() override {
		DX_API("reset command allocator") commandAllocator->Reset();
		DX_API("reset command list") commandList->Reset(commandAllocator.Get(), nullptr);

		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			renderTargets[swapChainBackBufferIndex].Get(),
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

		CD3DX12_CPU_DESCRIPTOR_HANDLE rHandle(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
			swapChainBackBufferIndex, rtvDescriptorHandleIncrementSize);
		CD3DX12_CPU_DESCRIPTOR_HANDLE dHandle(dsvHeap->GetCPUDescriptorHandleForHeapStart());

		commandList->RSSetViewports(1, &viewPort);
		commandList->RSSetScissorRects(1, &scissorRect);
		commandList->OMSetRenderTargets(1, &rHandle, FALSE, &dHandle);

		const float clearColor[] = { 0.05f, 0.06f, 0.08f, 1.0f };
		commandList->ClearRenderTargetView(rHandle, clearColor, 0, nullptr);
		commandList->ClearDepthStencilView(dHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		if (showReference) {
			commandList->SetGraphicsRootSignature(raymarchRootSig.Get());
			commandList->SetGraphicsRootConstantBufferView(0, frameCb.GetGPUVirtualAddress());
			commandList->SetGraphicsRootConstantBufferView(1, torusCb.GetGPUVirtualAddress());
			commandList->SetPipelineState(raymarchPso.Get());
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			commandList->DrawInstanced(3, 1, 0, 0);
		}

		if (dataValid && showSurface) {
			commandList->SetGraphicsRootSignature(surfaceRootSig.Get());
			commandList->SetGraphicsRootConstantBufferView(0, frameCb.GetGPUVirtualAddress());
			commandList->SetGraphicsRootUnorderedAccessView(1, surfaceVertexBuffer->GetGPUVirtualAddress());
			commandList->SetPipelineState(surfacePso.Get());
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			commandList->DrawInstanced(WindowTetCount * 6, 1, 0, 0); // windowed -- see extractSurfaceCS.hlsl
		}

		if (dataValid && showNodes) {
			commandList->SetGraphicsRootSignature(nodePointRootSig.Get());
			commandList->SetGraphicsRootConstantBufferView(0, frameCb.GetGPUVirtualAddress());
			commandList->SetGraphicsRootUnorderedAccessView(1, nodeCandidateLabelBuffer->GetGPUVirtualAddress());
			commandList->SetGraphicsRootUnorderedAccessView(2, nodePotentialBuffer->GetGPUVirtualAddress());
			commandList->SetGraphicsRootConstantBufferView(3, distanceGridCb.GetGPUVirtualAddress());
			commandList->SetPipelineState(nodePointPso.Get());
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			commandList->DrawInstanced(4, NodeCount, 0, 0);
		}

		if (dataValid && showPotentialSlice) {
			commandList->SetGraphicsRootSignature(footSliceRootSig.Get());
			commandList->SetGraphicsRootConstantBufferView(0, frameCb.GetGPUVirtualAddress());
			int clampedSliceIndex = sliceIndex < 0 ? 0 : (sliceIndex > (int)GridRes - 1 ? (int)GridRes - 1 : sliceIndex);
			struct { uint32_t axis; float coord; float colorScale; float pad; } sliceConsts = {
				(uint32_t)sliceAxis, (float)clampedSliceIndex * CellSize, potentialColorScale, 0.0f
			};
			commandList->SetGraphicsRoot32BitConstants(1, 4, &sliceConsts, 0);
			commandList->SetGraphicsRootUnorderedAccessView(2, nodeCandidateLabelBuffer->GetGPUVirtualAddress());
			commandList->SetGraphicsRootUnorderedAccessView(3, nodePotentialBuffer->GetGPUVirtualAddress());
			commandList->SetGraphicsRootConstantBufferView(4, distanceGridCb.GetGPUVirtualAddress());
			commandList->SetPipelineState(footSlicePso.Get());
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			commandList->DrawInstanced(3, 1, 0, 0);
		}

		if (pickedTetValid) {
			commandList->SetGraphicsRootSignature(wireframeRootSig.Get());
			commandList->SetGraphicsRootConstantBufferView(0, frameCb.GetGPUVirtualAddress());
			commandList->SetGraphicsRootConstantBufferView(1, pickedTetCb.GetGPUVirtualAddress());
			commandList->SetPipelineState(wireframePso.Get());
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
			commandList->DrawInstanced(12, 1, 0, 0);
		}

		BuildImGui();

		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			renderTargets[swapChainBackBufferIndex].Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

		DX_API("close graphics command list") commandList->Close();
	}

	virtual void Resize(int width, int height) override {
		Egg::SimpleApp::Resize(width, height);
	}

	virtual void Update(float dt, float T) override {
		using namespace Egg::Math;

		if (camera) camera->Animate(dt);

		frameCb.data.viewProjTransform = camera->GetViewMatrix() * camera->GetProjMatrix();
		frameCb.data.rayDirTransform   = camera->GetRayDirMatrix();
		frameCb.data.cameraPos         = float4(camera->GetEyePosition(), 1.0f);
		frameCb.data.raymarchParams    = float4((float)MaxMarchSteps, MaxMarchDist, potentialSizeScale, 0.0f);
		frameCb.data.pointParams       = float4(viewPort.Width, viewPort.Height, pointRadiusPx, hideUniformNodes ? 1.0f : 0.0f);
		frameCb.data.nodeFadeParams    = float4(nodeFadeExponent, nodeFadeStartDistance, 0.0f, 0.0f);
		frameCb.Upload();
	}

	virtual void ProcessMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override {
		if (camera) camera->ProcessMessage(hWnd, uMsg, wParam, lParam);

		if (uMsg == WM_MOUSEMOVE) {
			mouseX = (int)(short)LOWORD(lParam);
			mouseY = (int)(short)HIWORD(lParam);
		}

		if (uMsg == WM_RBUTTONDOWN && dataValid &&
			(!ImGui::GetCurrentContext() || !ImGui::GetIO().WantCaptureMouse)) {
			mouseX = (int)(short)LOWORD(lParam);
			mouseY = (int)(short)HIWORD(lParam);
			pickRequested = true;
		}

		if (uMsg == WM_KEYDOWN && ImGui::GetCurrentContext() && !ImGui::GetIO().WantCaptureKeyboard) {
			if (wParam == 'R') {
				needsReinit = true;
			} else if (wParam == 'C' && dataValid) {
				needsContinue = true;
			} else if (wParam >= '1' && wParam <= '9') {
				int digit = (int)(wParam - '0');
				iterations = digit;
				continueIterations = digit;
			}
		}
	}

	virtual void ReleaseSwapChainResources() override {
		Egg::SimpleApp::ReleaseSwapChainResources();
	}

	virtual void ReleaseResources() override {
		frameCb.ReleaseResources();
		distanceCb.ReleaseResources();
		distanceGridCb.ReleaseResources();
		torusCb.ReleaseResources();
		pickedTetCb.ReleaseResources();
		rasterLabelBuffer.Reset();
		nodeIsConnectingBuffer.Reset();
		nodeCandidateLabelBuffer.Reset();
		nodePotentialBuffer.Reset();
		nodePotentialScratchBuffer.Reset();
		nodeFrozenWinnerBuffer.Reset();
		surfaceVertexBuffer.Reset();
		nodeCurrentVolumeBuffer.Reset();
		imguiSrvHeap.Reset();
		raymarchRootSig.Reset();
		raymarchPso.Reset();
		nodePointRootSig.Reset();
		nodePointPso.Reset();
		surfaceRootSig.Reset();
		surfacePso.Reset();
		wireframeRootSig.Reset();
		wireframePso.Reset();
		Egg::SimpleApp::ReleaseResources();
	}
};
