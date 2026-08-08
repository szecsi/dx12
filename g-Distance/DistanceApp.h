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
#include "Shaders/DistanceFrameCb.hlsli"
#include "Shaders/PickedTetCb.hlsli"
#include "Shaders/TorusListCb.hlsli"

#include <vector>
#include <string>
#include <cstdio>

// Must match TorusListCb.ShapeKind's convention and rasterLabelCS.hlsl's
// branch on it. 0/1 (Torus/Ellipsoid) are analytic-SDF shapes, resolved via
// TorusSdf.hlsli as before. 2-5 are discrete grid-index patterns with no
// analytic SDF at all -- deliberately pathological "ambiguous cube" stress
// tests (a lone voxel; a straight line; two label-1 voxels touching only
// along a cube's face diagonal; touching only along a cube's body diagonal)
// -- rasterLabelCS.hlsl sets their labels directly from tid, bypassing
// ShapeSd/torii entirely. No meaningful raymarch reference exists for these
// (torii stays empty), so the background stays visible behind them.
enum DistanceTestShape {
	TestShape_Torus = 0,
	TestShape_Ellipsoid = 1,
	TestShape_SinglePoint = 2,
	TestShape_Line = 3,
	TestShape_DiagonalLine2D = 4,
	TestShape_DiagonalLine3D = 5,
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

	static constexpr uint GridRes  = GRID_RES;   // must match DistanceConfig.hlsli
	static constexpr float CellSize = CELL_SIZE; // must match DistanceConfig.hlsli
	static constexpr uint MaxIncidentTets = MAX_INCIDENT_TETS;

	static constexpr uint BDim   = GridRes - 1;
	static constexpr uint ACount = GridRes * GridRes * GridRes;
	static constexpr uint BCount = BDim * BDim * BDim;
	static constexpr uint NodeCount = ACount + BCount;

	// Disphenoid tet count -- see buildTetsCS.hlsl's derivation comment.
	static constexpr uint Nx = GridRes - 2;
	static constexpr uint Ni = GridRes - 1;
	static constexpr uint FacesPerOrientation = Nx * Ni * Ni;
	static constexpr uint TotalFaces = FacesPerOrientation * 3;
	static constexpr uint TetCount = TotalFaces * 4;

	static constexpr uint RasterGroups            = (GridRes + 3) / 4; // numthreads(4,4,4)
	static constexpr uint BuildTetsGroups         = (TotalFaces + THREAD_GROUP_SIZE - 1) / THREAD_GROUP_SIZE;
	static constexpr uint ClearIncidentGroups     = (NodeCount + THREAD_GROUP_SIZE - 1) / THREAD_GROUP_SIZE;
	static constexpr uint BuildIncidentGroups     = (TetCount + THREAD_GROUP_SIZE - 1) / THREAD_GROUP_SIZE;
	static constexpr uint BuildCandidatesAGroups  = (ACount + THREAD_GROUP_SIZE - 1) / THREAD_GROUP_SIZE;
	static constexpr uint BuildCandidatesBGroups  = (BCount + THREAD_GROUP_SIZE - 1) / THREAD_GROUP_SIZE;
	static constexpr uint AssignInterfaceGroups   = (TetCount + THREAD_GROUP_SIZE - 1) / THREAD_GROUP_SIZE;
	static constexpr uint SmoothnessGroups        = (NodeCount + THREAD_GROUP_SIZE - 1) / THREAD_GROUP_SIZE;
	static constexpr uint CommitGroups            = (NodeCount * 8 + THREAD_GROUP_SIZE - 1) / THREAD_GROUP_SIZE;

	static constexpr float MaxMarchDist = GridRes * CellSize * 3.0f;
	static constexpr int MaxMarchSteps = 256;

	uint frameCount = 0;

	Egg::Cam::FirstPerson::P camera;

protected:
	com_ptr<ID3D12CommandAllocator>    uploadAllocator;
	com_ptr<ID3D12GraphicsCommandList> uploadCommandList;
	Egg::Compute::Fence uploadFence;
	uint64_t uploadFenceValue = 0;

	// -- static lattice/topology data, built once at init (RunReinit only) --
	com_ptr<ID3D12Resource> rasterLabelBuffer;          // ACount uint: ground-truth A-node label
	com_ptr<ID3D12Resource> tetsBuffer;                 // TetCount uint4: (A0,A1,B0,B1) global node indices
	com_ptr<ID3D12Resource> nodeIncidentCountBuffer;    // NodeCount uint: reverse-adjacency degree
	com_ptr<ID3D12Resource> nodeIncidentTetsBuffer;     // NodeCount*MaxIncidentTets uint: reverse adjacency
	com_ptr<ID3D12Resource> tetFaceNeighborsBuffer;     // TetCount uint2: cross-orientation neighbor tets (SENTINEL_LABEL = none)

	// -- per-node candidate/potential state, (re)seeded at init, evolved by the outer loop --
	com_ptr<ID3D12Resource> nodeCandidateLabelBuffer;   // NodeCount*8 uint (SENTINEL_LABEL = unused slot)
	com_ptr<ID3D12Resource> nodePotentialBuffer;        // NodeCount*8 float, "current" (Jacobi read buffer)
	com_ptr<ID3D12Resource> nodePotentialScratchBuffer; // NodeCount*8 float, Jacobi write buffer
	com_ptr<ID3D12Resource> tetInterfacePairBuffer;     // TetCount uint2: current active (labelI,labelJ) per tet
	com_ptr<ID3D12Resource> surfaceVertexBuffer;        // TetCount*6 SurfaceVertex (pos+normal), render-only

	// -- volume conservation (MTV = Momentary Target Volume) --
	com_ptr<ID3D12Resource> nodeMTVBuffer;              // NodeCount float, "current" (mtv read buffer)
	com_ptr<ID3D12Resource> nodeMTVScratchBuffer;       // NodeCount float, diffusion+flip-adjustment output
	com_ptr<ID3D12Resource> nodePrevLabelBuffer;        // NodeCount uint: winning label as of the previous round (flip-detection baseline)
	com_ptr<ID3D12Resource> nodePrevLabelScratchBuffer; // NodeCount uint, next round's baseline
	com_ptr<ID3D12Resource> nodeFlipFlagBuffer;         // NodeCount uint (0/1), recomputed fresh every round
	com_ptr<ID3D12Resource> nodeFlipShareOldBuffer;     // NodeCount float: MTV/countOld, only meaningful where flagged
	com_ptr<ID3D12Resource> nodeFlipShareNewBuffer;     // NodeCount float: MTV/countNew, only meaningful where flagged
	com_ptr<ID3D12Resource> nodeCurrentVolumeBuffer;    // NodeCount float: node's own winning-label reconstructed volume, written by smoothnessJacobiCS every sweep, read by mtvDiffuseCS's pushback term
	com_ptr<ID3D12Resource> nodeSmoothPressureBuffer;   // NodeCount float: node's own Term-1-only Newton step, clamped, written by smoothnessJacobiCS every sweep, read by mtvDiffuseCS's smooth-pressure term

	Egg::Compute::ComputeShader rasterLabelCS;
	Egg::Compute::ComputeShader buildTetsCS;
	Egg::Compute::ComputeShader clearIncidentCS;
	Egg::Compute::ComputeShader buildIncidentCS;
	Egg::Compute::ComputeShader buildTetFaceNeighborsCS;
	Egg::Compute::ComputeShader buildCandidatesCS;
	Egg::Compute::ComputeShader assignInterfacePairsCS;
	Egg::Compute::ComputeShader smoothnessJacobiCS;
	Egg::Compute::ComputeShader commitPotentialCS;
	Egg::Compute::ComputeShader extractSurfaceCS;
	Egg::Compute::ComputeShader mtvSeedCS;
	Egg::Compute::ComputeShader mtvFlipDetectCS;
	Egg::Compute::ComputeShader mtvDiffuseCS;
	Egg::Compute::ComputeShader mtvCommitCS;

	com_ptr<ID3D12RootSignature> raymarchRootSig;
	com_ptr<ID3D12PipelineState> raymarchPso;
	com_ptr<ID3D12RootSignature> nodePointRootSig;
	com_ptr<ID3D12PipelineState> nodePointPso;
	com_ptr<ID3D12RootSignature> surfaceRootSig;
	com_ptr<ID3D12PipelineState> surfacePso;
	com_ptr<ID3D12RootSignature> wireframeRootSig;
	com_ptr<ID3D12PipelineState> wireframePso;

	Egg::ConstantBuffer<DistanceFrameCb> frameCb;
	Egg::ConstantBuffer<DistanceCb>      distanceCb;
	Egg::ConstantBuffer<TorusListCb>     torusCb;
	Egg::ConstantBuffer<PickedTetCb>     pickedTetCb;

	com_ptr<ID3D12DescriptorHeap> imguiSrvHeap;

	int testShapeKind = TestShape_Torus; // driven by the ImGui combo, applied on Reinitialize only (BuildShapeList)
	bool needsReinit = false;
	bool needsContinue = false;
	bool dataValid = false; // true once a Reinit/Continue has actually produced node data

	int iterations = 8;          // outer Lloyd-loop rounds on Reinitialize
	int continueIterations = 8;  // outer Lloyd-loop rounds on Continue
	int jacobiSweepsPerRound = 4; // inner linear-solve depth per outer round

	float smoothnessWeight = 1.0f;
	float marginWeight = 1.0f;
	float marginTarget = 0.5f;
	float regularizerWeight = 0.02f;
	float jacobiDiagEpsilon = 0.05f;
	float seedJitter = 0.05f;
	float ownLabelSeed = 1.0f;
	bool neutralBSeed = true; // B-nodes seed with pure jitter instead of a majority-vote confidence boost, see buildCandidatesCS.hlsl
	float maxPotentialStep = 0.002f; // hard per-sweep step clamp, see DistanceCb.hlsli -- user-confirmed stable value; 0.1 was not, likely because the all-edges connectivity change couples each unknown to more neighbors than the original fan-only gather did
	float volumeWeight = 5000.0f; // energy term 4 weight, see DistanceCb.hlsli -- needs to be this large (not ~1 like the other weights) to actually outweigh smoothness's gradient at a topologically point-like feature; see "Volume Weight" slider comment
	float mtvDiffusionRate = 0.25f;  // MTV diffusion rate per round, see DistanceCb.hlsli
	float volumePushbackRate = 0.25f; // lets a node's own current volume pull its MTV target toward reality, see DistanceCb.hlsli
	float smoothPressureRate = 1.0f;  // more direct/leading alternative to volumePushbackRate, see DistanceCb.hlsli
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
	uint pickedCornerLabels[4][8] = {};
	float pickedCornerPots[4][8] = {};
	float pickedCornerMTV[4] = {};             // NodeMTV, see mtvSeedCS/mtvDiffuseCS/mtvCommitCS
	float pickedCornerCurrentVolume[4] = {};   // NodeCurrentVolume, written by smoothnessJacobiCS's Term 4
	float pickedCornerSmoothPressure[4] = {};  // NodeSmoothPressure, written by smoothnessJacobiCS's Term 1 shadow accumulators
	uint pickedCornerPrevLabel[4] = {};        // NodePrevLabel, flip-detection baseline
	uint pickedCornerFlipFlag[4] = {};         // NodeFlipFlag, set by mtvFlipDetectCS this round
	float pickedCornerFlipShareOld[4] = {};    // NodeFlipShareOld, only meaningful if FlipFlag set
	float pickedCornerFlipShareNew[4] = {};    // NodeFlipShareNew, only meaningful if FlipFlag set

	bool showNodes = true;
	bool showSurface = true;
	bool showReference = true;
	bool hideUniformNodes = false; // hide nodes whose structural neighbors all share its current label

	// Test shape: same torus/ellipsoid scene as g-BCC/g-Aequor, centered in
	// the middle of the (positive-only) BCC lattice index space rather than
	// at the origin -- this lattice reuses g-BCC's APos/BPos convention
	// (indices start at 0), not g-Aequor's centered-at-origin one.
	void BuildShapeList() {
		using namespace Egg::Math;
		float3 center(GridRes * CellSize * 0.5f, GridRes * CellSize * 0.5f, GridRes * CellSize * 0.5f);

		torusCb.data.ShapeKind = (uint)testShapeKind;

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
			// empty (no raymarch reference makes sense for a lone voxel or
			// a 1-voxel-wide diagonal line anyway).
			torusCb.data.nTorii = 0;
			return;
		}

		torusCb.data.nTorii = 1;
		torusCb.data.torii[0].center      = center;
		torusCb.data.torii[0].axis        = float3(0, 1, 0);
		torusCb.data.torii[0].majorRadius = 6.0f;
		torusCb.data.torii[0].minorRadius = 2.5f;
		torusCb.data.torii[0].label       = 1;
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
		distanceCb.data.MTVDiffusionRate = mtvDiffusionRate;
		distanceCb.data.VolumePushbackRate = volumePushbackRate;
		distanceCb.data.SmoothPressureRate = smoothPressureRate;
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

	// Static lattice + topology build: rasterize A, build disphenoid tets,
	// build the node->tets reverse adjacency, seed candidate labels +
	// potentials. Everything here is independent of the solve's tunables and
	// never changes across outer-loop rounds, so it only ever runs once per
	// Reinitialize (never on Continue) -- see buildTetsCS.hlsl's comment on
	// why tet connectivity is built once, not per iteration.
	void RunTopologyBuild(com_ptr<ID3D12GraphicsCommandList>& cmd) {
		cmd->SetComputeRootSignature(rasterLabelCS.rootSig.Get());
		cmd->SetPipelineState(rasterLabelCS.pso.Get());
		cmd->SetComputeRootConstantBufferView(0, torusCb.GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(1, rasterLabelBuffer->GetGPUVirtualAddress());
		cmd->Dispatch(RasterGroups, RasterGroups, RasterGroups);
		cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(rasterLabelBuffer.Get()));

		cmd->SetComputeRootSignature(buildTetsCS.rootSig.Get());
		cmd->SetPipelineState(buildTetsCS.pso.Get());
		cmd->SetComputeRootUnorderedAccessView(0, tetsBuffer->GetGPUVirtualAddress());
		cmd->Dispatch(BuildTetsGroups, 1, 1);
		cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(tetsBuffer.Get()));

		cmd->SetComputeRootSignature(clearIncidentCS.rootSig.Get());
		cmd->SetPipelineState(clearIncidentCS.pso.Get());
		cmd->SetComputeRootUnorderedAccessView(0, nodeIncidentCountBuffer->GetGPUVirtualAddress());
		cmd->Dispatch(ClearIncidentGroups, 1, 1);
		cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(nodeIncidentCountBuffer.Get()));

		cmd->SetComputeRootSignature(buildIncidentCS.rootSig.Get());
		cmd->SetPipelineState(buildIncidentCS.pso.Get());
		cmd->SetComputeRootUnorderedAccessView(0, tetsBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(1, nodeIncidentCountBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(2, nodeIncidentTetsBuffer->GetGPUVirtualAddress());
		cmd->Dispatch(BuildIncidentGroups, 1, 1);
		{
			D3D12_RESOURCE_BARRIER b[] = {
				CD3DX12_RESOURCE_BARRIER::UAV(nodeIncidentCountBuffer.Get()),
				CD3DX12_RESOURCE_BARRIER::UAV(nodeIncidentTetsBuffer.Get()),
			};
			cmd->ResourceBarrier(_countof(b), b);
		}

		cmd->SetComputeRootSignature(buildTetFaceNeighborsCS.rootSig.Get());
		cmd->SetPipelineState(buildTetFaceNeighborsCS.pso.Get());
		cmd->SetComputeRootUnorderedAccessView(0, tetsBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(1, nodeIncidentCountBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(2, nodeIncidentTetsBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(3, tetFaceNeighborsBuffer->GetGPUVirtualAddress());
		cmd->Dispatch(BuildIncidentGroups, 1, 1); // same TetCount-based group count
		cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(tetFaceNeighborsBuffer.Get()));

		cmd->SetComputeRootSignature(buildCandidatesCS.rootSig.Get());
		cmd->SetPipelineState(buildCandidatesCS.pso.Get());
		cmd->SetComputeRootConstantBufferView(1, distanceCb.GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(2, rasterLabelBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(3, nodeCandidateLabelBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(4, nodePotentialBuffer->GetGPUVirtualAddress());
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

		// Seed MTV (A-nodes=1, B-nodes=0) + NodePrevLabel, once per
		// Reinitialize, right after candidates/potentials are (re)seeded.
		cmd->SetComputeRootSignature(mtvSeedCS.rootSig.Get());
		cmd->SetPipelineState(mtvSeedCS.pso.Get());
		cmd->SetComputeRootUnorderedAccessView(0, nodeCandidateLabelBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(1, nodePotentialBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(2, nodeMTVBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(3, nodePrevLabelBuffer->GetGPUVirtualAddress());
		cmd->Dispatch(SmoothnessGroups, 1, 1);
		{
			D3D12_RESOURCE_BARRIER b[] = {
				CD3DX12_RESOURCE_BARRIER::UAV(nodeMTVBuffer.Get()),
				CD3DX12_RESOURCE_BARRIER::UAV(nodePrevLabelBuffer.Get()),
			};
			cmd->ResourceBarrier(_countof(b), b);
		}
	}

	// One outer Lloyd-loop round: fix combinatorics (assignInterfacePairsCS),
	// then relax the resulting quadratic energy for jacobiSweepsPerRound
	// Jacobi sweeps (smoothnessJacobiCS + commitPotentialCS), then repeat.
	void RunOneRound(com_ptr<ID3D12GraphicsCommandList>& cmd) {
		cmd->SetComputeRootSignature(assignInterfacePairsCS.rootSig.Get());
		cmd->SetPipelineState(assignInterfacePairsCS.pso.Get());
		cmd->SetComputeRootUnorderedAccessView(0, tetsBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(1, nodeCandidateLabelBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(2, nodePotentialBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(3, tetInterfacePairBuffer->GetGPUVirtualAddress());
		cmd->Dispatch(BuildTetsGroups, 1, 1); // now one thread per FACE (TotalFaces), not per tet -- see assignInterfacePairsCS.hlsl
		cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(tetInterfacePairBuffer.Get()));

		int sweeps = jacobiSweepsPerRound > 0 ? jacobiSweepsPerRound : 1;
		for (int s = 0; s < sweeps; s++) {
			cmd->SetComputeRootSignature(smoothnessJacobiCS.rootSig.Get());
			cmd->SetPipelineState(smoothnessJacobiCS.pso.Get());
			cmd->SetComputeRootConstantBufferView(0, distanceCb.GetGPUVirtualAddress());
			cmd->SetComputeRootUnorderedAccessView(1, tetsBuffer->GetGPUVirtualAddress());
			cmd->SetComputeRootUnorderedAccessView(2, tetInterfacePairBuffer->GetGPUVirtualAddress());
			cmd->SetComputeRootUnorderedAccessView(3, nodeCandidateLabelBuffer->GetGPUVirtualAddress());
			cmd->SetComputeRootUnorderedAccessView(4, nodePotentialBuffer->GetGPUVirtualAddress());
			cmd->SetComputeRootUnorderedAccessView(5, nodePotentialScratchBuffer->GetGPUVirtualAddress());
			cmd->SetComputeRootUnorderedAccessView(6, nodeIncidentCountBuffer->GetGPUVirtualAddress());
			cmd->SetComputeRootUnorderedAccessView(7, nodeIncidentTetsBuffer->GetGPUVirtualAddress());
			cmd->SetComputeRootUnorderedAccessView(8, tetFaceNeighborsBuffer->GetGPUVirtualAddress());
			cmd->SetComputeRootUnorderedAccessView(9, nodeMTVBuffer->GetGPUVirtualAddress());
			cmd->SetComputeRootUnorderedAccessView(10, nodeCurrentVolumeBuffer->GetGPUVirtualAddress());
			cmd->SetComputeRootUnorderedAccessView(11, nodeSmoothPressureBuffer->GetGPUVirtualAddress());
			cmd->Dispatch(SmoothnessGroups, 1, 1);
			{
				D3D12_RESOURCE_BARRIER b[] = {
					CD3DX12_RESOURCE_BARRIER::UAV(nodePotentialScratchBuffer.Get()),
					CD3DX12_RESOURCE_BARRIER::UAV(nodeCurrentVolumeBuffer.Get()),
					CD3DX12_RESOURCE_BARRIER::UAV(nodeSmoothPressureBuffer.Get()),
				};
				cmd->ResourceBarrier(_countof(b), b);
			}

			cmd->SetComputeRootSignature(commitPotentialCS.rootSig.Get());
			cmd->SetPipelineState(commitPotentialCS.pso.Get());
			cmd->SetComputeRootUnorderedAccessView(0, nodePotentialScratchBuffer->GetGPUVirtualAddress());
			cmd->SetComputeRootUnorderedAccessView(1, nodePotentialBuffer->GetGPUVirtualAddress());
			cmd->Dispatch(CommitGroups, 1, 1);
			cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(nodePotentialBuffer.Get()));
		}

		// MTV round: once per outer round, after this round's Jacobi sweeps
		// have settled the potentials -- flip-detect (gather), diffuse
		// (gather + apply flip shares), commit (scratch->main). Feeds
		// smoothnessJacobiCS's Term 4 for the NEXT round's sweeps.
		cmd->SetComputeRootSignature(mtvFlipDetectCS.rootSig.Get());
		cmd->SetPipelineState(mtvFlipDetectCS.pso.Get());
		cmd->SetComputeRootUnorderedAccessView(0, tetsBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(1, nodeIncidentCountBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(2, nodeIncidentTetsBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(3, nodeCandidateLabelBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(4, nodePotentialBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(5, nodePrevLabelBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(6, nodeMTVBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(7, nodeFlipFlagBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(8, nodeFlipShareOldBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(9, nodeFlipShareNewBuffer->GetGPUVirtualAddress());
		cmd->Dispatch(SmoothnessGroups, 1, 1);
		{
			D3D12_RESOURCE_BARRIER b[] = {
				CD3DX12_RESOURCE_BARRIER::UAV(nodeFlipFlagBuffer.Get()),
				CD3DX12_RESOURCE_BARRIER::UAV(nodeFlipShareOldBuffer.Get()),
				CD3DX12_RESOURCE_BARRIER::UAV(nodeFlipShareNewBuffer.Get()),
			};
			cmd->ResourceBarrier(_countof(b), b);
		}

		cmd->SetComputeRootSignature(mtvDiffuseCS.rootSig.Get());
		cmd->SetPipelineState(mtvDiffuseCS.pso.Get());
		cmd->SetComputeRootConstantBufferView(0, distanceCb.GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(1, tetsBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(2, nodeIncidentCountBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(3, nodeIncidentTetsBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(4, nodeCandidateLabelBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(5, nodePotentialBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(6, nodePrevLabelBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(7, nodeMTVBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(8, nodeFlipFlagBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(9, nodeFlipShareOldBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(10, nodeFlipShareNewBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(11, nodeMTVScratchBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(12, nodePrevLabelScratchBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(13, nodeCurrentVolumeBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(14, nodeSmoothPressureBuffer->GetGPUVirtualAddress());
		cmd->Dispatch(SmoothnessGroups, 1, 1);
		{
			D3D12_RESOURCE_BARRIER b[] = {
				CD3DX12_RESOURCE_BARRIER::UAV(nodeMTVScratchBuffer.Get()),
				CD3DX12_RESOURCE_BARRIER::UAV(nodePrevLabelScratchBuffer.Get()),
			};
			cmd->ResourceBarrier(_countof(b), b);
		}

		cmd->SetComputeRootSignature(mtvCommitCS.rootSig.Get());
		cmd->SetPipelineState(mtvCommitCS.pso.Get());
		cmd->SetComputeRootUnorderedAccessView(0, nodeMTVScratchBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(1, nodeMTVBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(2, nodePrevLabelScratchBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(3, nodePrevLabelBuffer->GetGPUVirtualAddress());
		cmd->Dispatch(SmoothnessGroups, 1, 1);
		{
			D3D12_RESOURCE_BARRIER b[] = {
				CD3DX12_RESOURCE_BARRIER::UAV(nodeMTVBuffer.Get()),
				CD3DX12_RESOURCE_BARRIER::UAV(nodePrevLabelBuffer.Get()),
			};
			cmd->ResourceBarrier(_countof(b), b);
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
		cmd->SetComputeRootUnorderedAccessView(0, tetsBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(1, tetInterfacePairBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(2, nodeCandidateLabelBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(3, nodePotentialBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(4, surfaceVertexBuffer->GetGPUVirtualAddress());
		cmd->Dispatch(AssignInterfaceGroups, 1, 1); // one thread per tet (TetCount)
		cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(surfaceVertexBuffer.Get()));
	}

	// Full reseed: rebuild the test scene, rasterize + rebuild lattice
	// topology from scratch, then relax for `iterations` outer rounds.
	void RunReinit() {
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

	// CPU mirror of DistanceLattice.hlsli's NodeWorldPos -- used only by the
	// tet-picking debug tool below, which needs node positions on the CPU
	// side to brute-force-search for the tet enclosing a picked world point.
	static Egg::Math::float3 CpuNodeWorldPos(uint g) {
		using namespace Egg::Math;
		if (g < ACount) {
			uint k = g / (GridRes * GridRes);
			uint rem = g % (GridRes * GridRes);
			uint j = rem / GridRes;
			uint i = rem % GridRes;
			return float3((float)i, (float)j, (float)k) * CellSize;
		} else {
			uint l = g - ACount;
			uint k = l / (BDim * BDim);
			uint rem = l % (BDim * BDim);
			uint j = rem / BDim;
			uint i = rem % BDim;
			return (float3((float)i, (float)j, (float)k) + float3(0.5f, 0.5f, 0.5f)) * CellSize;
		}
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

	// Brute-force point-in-tet search: reads back the (static, small)
	// Tets buffer fresh every call -- this is a rare, user-triggered debug
	// operation, not a per-frame cost, so simplicity/correctness wins over
	// caching a CPU-side copy across Reinitialize/Continue calls.
	uint FindEnclosingTet(const Egg::Math::float3& point) {
		using namespace Egg::Math;
		UINT64 tetsBytes = (UINT64)TetCount * sizeof(UINT) * 4;
		com_ptr<ID3D12Resource> readback = CreateReadbackBuffer(device.Get(), tetsBytes);

		DX_API("reset upload allocator (pick)") uploadAllocator->Reset();
		DX_API("reset upload command list (pick)") uploadCommandList->Reset(uploadAllocator.Get(), nullptr);
		auto& cmd = uploadCommandList;
		cmd->CopyBufferRegion(readback.Get(), 0, tetsBuffer.Get(), 0, tetsBytes);
		DX_API("close upload command list (pick)") cmd->Close();
		{
			ID3D12CommandList* lists[] = { cmd.Get() };
			commandQueue->ExecuteCommandLists(1, lists);
		}
		uploadFence.signal(commandQueue, ++uploadFenceValue);
		uploadFence.cpuWait();

		UINT* tets = nullptr;
		readback->Map(0, nullptr, (void**)&tets);

		uint found = 0xFFFFFFFFu;
		const float eps = 0.02f;
		for (uint t = 0; t < TetCount; t++) {
			uint a0 = tets[t * 4 + 0], a1 = tets[t * 4 + 1], b0 = tets[t * 4 + 2], b1 = tets[t * 4 + 3];
			float3 P0 = CpuNodeWorldPos(a0);
			float3 P1 = CpuNodeWorldPos(a1);
			float3 P2 = CpuNodeWorldPos(b0);
			float3 P3 = CpuNodeWorldPos(b1);
			float4 bary = BarycentricOf(point, P0, P1, P2, P3);
			if (bary.x >= -eps && bary.y >= -eps && bary.z >= -eps && bary.w >= -eps) {
				found = t;
				pickedTetCb.data.Corner0 = float4(P0, 1.0f);
				pickedTetCb.data.Corner1 = float4(P1, 1.0f);
				pickedTetCb.data.Corner2 = float4(P2, 1.0f);
				pickedTetCb.data.Corner3 = float4(P3, 1.0f);
				pickedTetNodes[0] = a0; pickedTetNodes[1] = a1; pickedTetNodes[2] = b0; pickedTetNodes[3] = b1;
				break;
			}
		}

		readback->Unmap(0, nullptr);
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

	// Reads back the assigned interface pair, full 8-slot candidate/
	// potential arrays, and the MTV/volume-conservation fields (MTV,
	// CurrentVolume, SmoothPressure, PrevLabel, flip flag/shares) for the
	// picked tet's 4 corners, for the "Picked Tet" GUI panel -- lets you
	// directly inspect the actual solved numbers at a specific wedge
	// inset/outcrop instead of guessing from the picture.
	void ReadBackPickedTetDiagnostics() {
		UINT64 pairBytes = (UINT64)TetCount * sizeof(UINT) * 2;
		UINT64 candBytes = (UINT64)NodeCount * 8 * sizeof(UINT);
		UINT64 potBytes = (UINT64)NodeCount * 8 * sizeof(float);
		UINT64 nodeFloatBytes = (UINT64)NodeCount * sizeof(float);
		UINT64 nodeUintBytes = (UINT64)NodeCount * sizeof(UINT);
		com_ptr<ID3D12Resource> rbPair = CreateReadbackBuffer(device.Get(), pairBytes);
		com_ptr<ID3D12Resource> rbCand = CreateReadbackBuffer(device.Get(), candBytes);
		com_ptr<ID3D12Resource> rbPot = CreateReadbackBuffer(device.Get(), potBytes);
		com_ptr<ID3D12Resource> rbMtv = CreateReadbackBuffer(device.Get(), nodeFloatBytes);
		com_ptr<ID3D12Resource> rbCurVol = CreateReadbackBuffer(device.Get(), nodeFloatBytes);
		com_ptr<ID3D12Resource> rbSmoothPressure = CreateReadbackBuffer(device.Get(), nodeFloatBytes);
		com_ptr<ID3D12Resource> rbPrevLabel = CreateReadbackBuffer(device.Get(), nodeUintBytes);
		com_ptr<ID3D12Resource> rbFlipFlag = CreateReadbackBuffer(device.Get(), nodeUintBytes);
		com_ptr<ID3D12Resource> rbFlipShareOld = CreateReadbackBuffer(device.Get(), nodeFloatBytes);
		com_ptr<ID3D12Resource> rbFlipShareNew = CreateReadbackBuffer(device.Get(), nodeFloatBytes);

		DX_API("reset upload allocator (pick diag)") uploadAllocator->Reset();
		DX_API("reset upload command list (pick diag)") uploadCommandList->Reset(uploadAllocator.Get(), nullptr);
		auto& cmd = uploadCommandList;
		cmd->CopyBufferRegion(rbPair.Get(), 0, tetInterfacePairBuffer.Get(), 0, pairBytes);
		cmd->CopyBufferRegion(rbCand.Get(), 0, nodeCandidateLabelBuffer.Get(), 0, candBytes);
		cmd->CopyBufferRegion(rbPot.Get(), 0, nodePotentialBuffer.Get(), 0, potBytes);
		cmd->CopyBufferRegion(rbMtv.Get(), 0, nodeMTVBuffer.Get(), 0, nodeFloatBytes);
		cmd->CopyBufferRegion(rbCurVol.Get(), 0, nodeCurrentVolumeBuffer.Get(), 0, nodeFloatBytes);
		cmd->CopyBufferRegion(rbSmoothPressure.Get(), 0, nodeSmoothPressureBuffer.Get(), 0, nodeFloatBytes);
		cmd->CopyBufferRegion(rbPrevLabel.Get(), 0, nodePrevLabelBuffer.Get(), 0, nodeUintBytes);
		cmd->CopyBufferRegion(rbFlipFlag.Get(), 0, nodeFlipFlagBuffer.Get(), 0, nodeUintBytes);
		cmd->CopyBufferRegion(rbFlipShareOld.Get(), 0, nodeFlipShareOldBuffer.Get(), 0, nodeFloatBytes);
		cmd->CopyBufferRegion(rbFlipShareNew.Get(), 0, nodeFlipShareNewBuffer.Get(), 0, nodeFloatBytes);
		DX_API("close upload command list (pick diag)") cmd->Close();
		{
			ID3D12CommandList* lists[] = { cmd.Get() };
			commandQueue->ExecuteCommandLists(1, lists);
		}
		uploadFence.signal(commandQueue, ++uploadFenceValue);
		uploadFence.cpuWait();

		UINT* pair = nullptr;
		UINT* cand = nullptr;
		float* pot = nullptr;
		float* mtv = nullptr;
		float* curVol = nullptr;
		float* smoothPressure = nullptr;
		UINT* prevLabel = nullptr;
		UINT* flipFlag = nullptr;
		float* flipShareOld = nullptr;
		float* flipShareNew = nullptr;
		rbPair->Map(0, nullptr, (void**)&pair);
		rbCand->Map(0, nullptr, (void**)&cand);
		rbPot->Map(0, nullptr, (void**)&pot);
		rbMtv->Map(0, nullptr, (void**)&mtv);
		rbCurVol->Map(0, nullptr, (void**)&curVol);
		rbSmoothPressure->Map(0, nullptr, (void**)&smoothPressure);
		rbPrevLabel->Map(0, nullptr, (void**)&prevLabel);
		rbFlipFlag->Map(0, nullptr, (void**)&flipFlag);
		rbFlipShareOld->Map(0, nullptr, (void**)&flipShareOld);
		rbFlipShareNew->Map(0, nullptr, (void**)&flipShareNew);

		pickedInterfaceLabelI = pair[pickedTetIndex * 2 + 0];
		pickedInterfaceLabelJ = pair[pickedTetIndex * 2 + 1];
		for (uint c = 0; c < 4; c++) {
			uint node = pickedTetNodes[c];
			for (uint s = 0; s < 8; s++) {
				pickedCornerLabels[c][s] = cand[node * 8 + s];
				pickedCornerPots[c][s] = pot[node * 8 + s];
			}
			pickedCornerMTV[c] = mtv[node];
			pickedCornerCurrentVolume[c] = curVol[node];
			pickedCornerSmoothPressure[c] = smoothPressure[node];
			pickedCornerPrevLabel[c] = prevLabel[node];
			pickedCornerFlipFlag[c] = flipFlag[node];
			pickedCornerFlipShareOld[c] = flipShareOld[node];
			pickedCornerFlipShareNew[c] = flipShareNew[node];
		}
		pickedDiagnosticsValid = true;

		rbPair->Unmap(0, nullptr);
		rbCand->Unmap(0, nullptr);
		rbPot->Unmap(0, nullptr);
		rbMtv->Unmap(0, nullptr);
		rbCurVol->Unmap(0, nullptr);
		rbSmoothPressure->Unmap(0, nullptr);
		rbPrevLabel->Unmap(0, nullptr);
		rbFlipFlag->Unmap(0, nullptr);
		rbFlipShareOld->Unmap(0, nullptr);
		rbFlipShareNew->Unmap(0, nullptr);
	}

	void BuildImGui() {
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::SetNextWindowSize(ImVec2(280, 0), ImGuiCond_FirstUseEver);
		ImGui::Begin("g-Distance Controls");
		static const char* testShapeItems[] = {
			"Torus", "Ellipsoid", "Single Point", "Line", "Diagonal Line 2D", "Diagonal Line 3D"
		};
		ImGui::Combo("Test Shape", &testShapeKind, testShapeItems, IM_ARRAYSIZE(testShapeItems));
		ImGui::TextDisabled("(applied on Reinitialize)");

		ImGui::SliderInt("Iterations", &iterations, 0, 64);
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
					for (uint s = 0; s < 8; s++) {
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
					ImGui::Text("      MTV=%.4f  CurVol=%.4f  SmoothPressure=%.5f  PrevLabel=%u%s",
						pickedCornerMTV[c], pickedCornerCurrentVolume[c], pickedCornerSmoothPressure[c],
						pickedCornerPrevLabel[c],
						pickedCornerFlipFlag[c] ? "  FLIPPED" : "");
					if (pickedCornerFlipFlag[c]) {
						ImGui::Text("      FlipShareOld=%.4f  FlipShareNew=%.4f",
							pickedCornerFlipShareOld[c], pickedCornerFlipShareNew[c]);
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
						sprintf_s(line, sizeof(line), "%s (node %u, %s): phi_i=%s%.6f  phi_j=%s%.6f  g=%.6f\r\n"
							"    MTV=%.6f  CurVol=%.6f  SmoothPressure=%.6f  PrevLabel=%u  FlipFlag=%u  FlipShareOld=%.6f  FlipShareNew=%.6f\r\n",
							cornerNames[c], node, (node < ACount) ? "A" : "B",
							hasI[c] ? "" : "*", phiI[c],
							hasJ[c] ? "" : "*", phiJ[c],
							phiI[c] - phiJ[c],
							pickedCornerMTV[c], pickedCornerCurrentVolume[c], pickedCornerSmoothPressure[c],
							pickedCornerPrevLabel[c], pickedCornerFlipFlag[c],
							pickedCornerFlipShareOld[c], pickedCornerFlipShareNew[c]);
						report += line;
					}
					ImGui::SetClipboardText(report.c_str());
				}
			}
		}

		if (ImGui::CollapsingHeader("Energy Weights")) {
			ImGui::SliderFloat("Smoothness Weight", &smoothnessWeight, 0.0f, 10.0f);
			ImGui::SliderFloat("Margin Weight", &marginWeight, 0.0f, 10.0f);
			ImGui::SliderFloat("Margin Target", &marginTarget, 0.0f, 2.0f);
			ImGui::SliderFloat("Regularizer Weight", &regularizerWeight, 0.0f, 1.0f);
			ImGui::SliderFloat("Jacobi Diag Epsilon", &jacobiDiagEpsilon, 0.001f, 1.0f);
			ImGui::SliderFloat("Seed Jitter", &seedJitter, 0.0f, 1.0f);
			ImGui::SliderFloat("Own Label Seed", &ownLabelSeed, 0.0f, 5.0f);
			ImGui::Checkbox("Neutral B Seed (+jitter)", &neutralBSeed);
			ImGui::TextDisabled("(applied on Reinitialize -- off = old majority-vote B seed)");
			ImGui::SliderFloat("Max Potential Step", &maxPotentialStep, 0.0001f, 0.1f, "%.4f");
			// Range is huge (not 0..5 like the other weights) because Term 4's
			// natural gradient magnitude is orders of magnitude smaller than
			// smoothness's (TetShapeGradients on these thin BCC disphenoids
			// produces large shape-gradient differences, while the volume
			// term's K=VSide/sumSide stays O(0.1-1)) -- confirmed via direct
			// GPU-readback probe (kSum0/diag0) that ~800-5000 is the range
			// where it actually starts overpowering smoothness's perpetual,
			// never-converging push at a topologically point-like feature
			// (an isolated voxel can't have a locally planar interface, so
			// smoothness never settles there and keeps eroding it every
			// sweep, forever, regardless of round count).
			ImGui::SliderFloat("Volume Weight", &volumeWeight, 0.0f, 10000.0f, "%.1f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderFloat("MTV Diffusion Rate", &mtvDiffusionRate, 0.0f, 1.0f);
			ImGui::SliderFloat("Volume Pushback Rate", &volumePushbackRate, 0.0f, 1.0f);
			ImGui::SliderFloat("Smooth Pressure Rate", &smoothPressureRate, 0.0f, 10.0f);
		}

		ImGui::SliderFloat("Point Radius", &pointRadiusPx, 0.5f, 10.0f);
		ImGui::SliderFloat("Potential Size Scale", &potentialSizeScale, 0.05f, 5.0f);
		ImGui::SliderFloat("Node Depth Fade Exponent", &nodeFadeExponent, 0.0f, 10.0f);
		ImGui::SliderFloat("Node Fade Start Distance", &nodeFadeStartDistance, 0.0f, 3.0f);

		ImGui::Separator();
		ImGui::Text("A nodes: %u   B nodes: %u   Tets: %u", ACount, BCount, TetCount);
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
		torusCb.CreateResources(device.Get());
		pickedTetCb.CreateResources(device.Get());

		D3D12_DESCRIPTOR_HEAP_DESC imguiHeapDesc = {};
		imguiHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		imguiHeapDesc.NumDescriptors = 1;
		imguiHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		DX_API("create imgui srv heap")
			device->CreateDescriptorHeap(&imguiHeapDesc, IID_PPV_ARGS(imguiSrvHeap.GetAddressOf()));

		rasterLabelBuffer          = CreateRawUavBuffer(device.Get(), (UINT64)ACount * sizeof(UINT), L"rasterLabelBuffer");
		tetsBuffer                 = CreateRawUavBuffer(device.Get(), (UINT64)TetCount * sizeof(UINT) * 4, L"tetsBuffer");
		nodeIncidentCountBuffer    = CreateRawUavBuffer(device.Get(), (UINT64)NodeCount * sizeof(UINT), L"nodeIncidentCountBuffer");
		nodeIncidentTetsBuffer     = CreateRawUavBuffer(device.Get(), (UINT64)NodeCount * MaxIncidentTets * sizeof(UINT), L"nodeIncidentTetsBuffer");
		tetFaceNeighborsBuffer     = CreateRawUavBuffer(device.Get(), (UINT64)TetCount * sizeof(UINT) * 2, L"tetFaceNeighborsBuffer");
		nodeCandidateLabelBuffer   = CreateRawUavBuffer(device.Get(), (UINT64)NodeCount * 8 * sizeof(UINT), L"nodeCandidateLabelBuffer");
		nodePotentialBuffer        = CreateRawUavBuffer(device.Get(), (UINT64)NodeCount * 8 * sizeof(float), L"nodePotentialBuffer");
		nodePotentialScratchBuffer = CreateRawUavBuffer(device.Get(), (UINT64)NodeCount * 8 * sizeof(float), L"nodePotentialScratchBuffer");
		tetInterfacePairBuffer     = CreateRawUavBuffer(device.Get(), (UINT64)TetCount * sizeof(UINT) * 2, L"tetInterfacePairBuffer");
		surfaceVertexBuffer        = CreateRawUavBuffer(device.Get(), (UINT64)TetCount * 6 * sizeof(float) * 6, L"surfaceVertexBuffer");
		nodeMTVBuffer              = CreateRawUavBuffer(device.Get(), (UINT64)NodeCount * sizeof(float), L"nodeMTVBuffer");
		nodeMTVScratchBuffer       = CreateRawUavBuffer(device.Get(), (UINT64)NodeCount * sizeof(float), L"nodeMTVScratchBuffer");
		nodePrevLabelBuffer        = CreateRawUavBuffer(device.Get(), (UINT64)NodeCount * sizeof(UINT), L"nodePrevLabelBuffer");
		nodePrevLabelScratchBuffer = CreateRawUavBuffer(device.Get(), (UINT64)NodeCount * sizeof(UINT), L"nodePrevLabelScratchBuffer");
		nodeFlipFlagBuffer         = CreateRawUavBuffer(device.Get(), (UINT64)NodeCount * sizeof(UINT), L"nodeFlipFlagBuffer");
		nodeFlipShareOldBuffer     = CreateRawUavBuffer(device.Get(), (UINT64)NodeCount * sizeof(float), L"nodeFlipShareOldBuffer");
		nodeFlipShareNewBuffer     = CreateRawUavBuffer(device.Get(), (UINT64)NodeCount * sizeof(float), L"nodeFlipShareNewBuffer");
		nodeCurrentVolumeBuffer    = CreateRawUavBuffer(device.Get(), (UINT64)NodeCount * sizeof(float), L"nodeCurrentVolumeBuffer");
		nodeSmoothPressureBuffer   = CreateRawUavBuffer(device.Get(), (UINT64)NodeCount * sizeof(float), L"nodeSmoothPressureBuffer");

		rasterLabelCS.createResources(device, "Shaders/rasterLabelCS.cso");
		buildTetsCS.createResources(device, "Shaders/buildTetsCS.cso");
		clearIncidentCS.createResources(device, "Shaders/clearIncidentCS.cso");
		buildIncidentCS.createResources(device, "Shaders/buildIncidentCS.cso");
		buildTetFaceNeighborsCS.createResources(device, "Shaders/buildTetFaceNeighborsCS.cso");
		buildCandidatesCS.createResources(device, "Shaders/buildCandidatesCS.cso");
		assignInterfacePairsCS.createResources(device, "Shaders/assignInterfacePairsCS.cso");
		smoothnessJacobiCS.createResources(device, "Shaders/smoothnessJacobiCS.cso");
		commitPotentialCS.createResources(device, "Shaders/commitPotentialCS.cso");
		extractSurfaceCS.createResources(device, "Shaders/extractSurfaceCS.cso");
		mtvSeedCS.createResources(device, "Shaders/mtvSeedCS.cso");
		mtvFlipDetectCS.createResources(device, "Shaders/mtvFlipDetectCS.cso");
		mtvDiffuseCS.createResources(device, "Shaders/mtvDiffuseCS.cso");
		mtvCommitCS.createResources(device, "Shaders/mtvCommitCS.cso");

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
			commandList->DrawInstanced(TetCount * 6, 1, 0, 0);
		}

		if (dataValid && showNodes) {
			commandList->SetGraphicsRootSignature(nodePointRootSig.Get());
			commandList->SetGraphicsRootConstantBufferView(0, frameCb.GetGPUVirtualAddress());
			commandList->SetGraphicsRootUnorderedAccessView(1, nodeCandidateLabelBuffer->GetGPUVirtualAddress());
			commandList->SetGraphicsRootUnorderedAccessView(2, nodePotentialBuffer->GetGPUVirtualAddress());
			commandList->SetPipelineState(nodePointPso.Get());
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			commandList->DrawInstanced(4, NodeCount, 0, 0);
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
		torusCb.ReleaseResources();
		pickedTetCb.ReleaseResources();
		rasterLabelBuffer.Reset();
		tetsBuffer.Reset();
		nodeIncidentCountBuffer.Reset();
		nodeIncidentTetsBuffer.Reset();
		tetFaceNeighborsBuffer.Reset();
		nodeCandidateLabelBuffer.Reset();
		nodePotentialBuffer.Reset();
		nodePotentialScratchBuffer.Reset();
		tetInterfacePairBuffer.Reset();
		surfaceVertexBuffer.Reset();
		nodeMTVBuffer.Reset();
		nodeMTVScratchBuffer.Reset();
		nodePrevLabelBuffer.Reset();
		nodePrevLabelScratchBuffer.Reset();
		nodeFlipFlagBuffer.Reset();
		nodeFlipShareOldBuffer.Reset();
		nodeFlipShareNewBuffer.Reset();
		nodeCurrentVolumeBuffer.Reset();
		nodeSmoothPressureBuffer.Reset();
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
