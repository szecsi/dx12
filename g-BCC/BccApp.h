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

#include "Shaders/TorusListCb.hlsli"
#include "Shaders/BccFrameCb.hlsli"
#include "Shaders/FootVizCb.hlsli"
#include "Shaders/QuadricOptimizationCb.hlsli"

#include <vector>
#include <utility>

// Must match RENDER_MODE_LABELS/RENDER_MODE_NORMALS/RENDER_MODE_DISTANCE in raymarchPS.hlsl.
enum BccRenderMode { RenderMode_Labels = 0, RenderMode_Normals = 1, RenderMode_Distance = 2 };

// Which compute pipeline populates A0/B0: propagate from seeded boundary
// voxels (JFA, approximate away from the seeds), compute the exact nearest
// torus surface directly at every lattice point (Analytic, no propagation --
// only possible because the test scene is parametric torii), run JFA and
// then refine B's result with the multiphase label-potential filter
// (JfaFilter -- an approximation of Analytic quality that doesn't need an
// analytic SDF, see labelPotential*CS.hlsl), or run JFA and then relax every
// node's footvector + a per-node curvature tensor with the quadric
// Gauss-Newton scheme (QuadricOptimization, see QuadricFootField.hlsli /
// CSFootpointGaussNewton.hlsl / CSTransportTensor.hlsl / CSTensorGradient.hlsl).
enum BccInitMethod { InitMethod_JFA = 0, InitMethod_Analytic = 1, InitMethod_JfaFilter = 2, InitMethod_QuadricOptimization = 3 };

// Must match TorusListCb.ShapeKind's 0/1 convention.
enum BccTestShape { TestShape_Torus = 0, TestShape_Ellipsoid = 1 };

// A BCC (body-centered-cubic) lattice volume made of two interleaved simple-
// cubic 3D textures: A at integer cell corners, B at cell centers (offset by
// half a cell diagonal). Both are rasterized with integer labels (a handful
// of test tori or an ellipsoid, see BuildShapeList()), then turned into a multi-label
// "footvector field" by the Jump Flood Algorithm: per lattice point, the
// vector to the nearest label interface plus the labels on either side of it.
// Visualized by sphere-tracing the field every frame. The async compute
// queue/fence-chain plumbing mirrors g-Retam's, ready for future per-frame
// passes that smooth the footvector field (not implemented yet).
class BccApp : public Egg::SimpleApp {

	static constexpr uint GridRes  = 48;   // must match bccCommon.hlsli
	static constexpr float CellSize = 1.0f; // must match bccCommon.hlsli
	static constexpr uint ThreadGroups = (GridRes + 3) / 4; // numthreads(4,4,4)

	static constexpr float MaxMarchDist = GridRes * CellSize * 3.0f;
	static constexpr int   MaxMarchSteps = 256;

	uint frameCount;

	Egg::Cam::FirstPerson::P camera;

protected:

	com_ptr<ID3D12CommandQueue>  computeCommandQueue;
	std::vector<com_ptr<ID3D12CommandAllocator>>     computeAllocators;
	std::vector<com_ptr<ID3D12GraphicsCommandList>>  computeCommandLists;

	com_ptr<ID3D12CommandAllocator>    uploadAllocator;
	com_ptr<ID3D12GraphicsCommandList> uploadCommandList;

	Egg::Compute::Fence      uploadFence;
	uint64_t                 uploadFenceValue = 0; // RunInitPasses()/CreateResources() can each signal multiple times
	Egg::Compute::FenceChain computeFenceChain;
	Egg::Compute::FenceChain graphicsFenceChain;

	// GridSlot indexes both gridTex[] and the UAV/SRV heaps. A0/B0 are the
	// externally-meaningful "grid A" / "grid B"; A1/B1 are JFA ping-pong
	// scratch, always resolved back into A0/B0 by jfaFinalizeCS. AFoot/BFoot
	// hold the *exact* (unquantized) analytic footpoint per lattice point --
	// bit-reinterpreted float3s stored in a uint4 texture, same trick as the
	// asuint(dist) channel elsewhere -- written only by analyticInitCS (JFA
	// has no continuous foot point to store, only ever the quantized seed
	// lattice site already in channel2 of A0/B0). Existing compute shaders'
	// root signatures still declare a 4-descriptor table, so they simply
	// don't see/touch these two extra heap slots.
	enum GridSlot { SlotA0 = 0, SlotA1 = 1, SlotB0 = 2, SlotB1 = 3, SlotAFoot = 4, SlotBFoot = 5, SlotCount = 6 };

	com_ptr<ID3D12Resource>       gridTex[SlotCount];
	com_ptr<ID3D12DescriptorHeap> uavHeap; // 4 UAV descriptors, compute-visible
	com_ptr<ID3D12DescriptorHeap> srvHeap; // 4 SRV descriptors, for the raymarch PS

	Egg::Compute::ComputeShader torusInitCS;
	Egg::Compute::ComputeShader analyticInitCS;
	Egg::Compute::ComputeShader jfaSeedCS;
	Egg::Compute::ComputeShader jfaStepCS;
	Egg::Compute::ComputeShader jfaFinalizeCS;
	Egg::Compute::ComputeShader labelPotentialInitCS;
	Egg::Compute::ComputeShader labelPotentialDiffuseCS;
	Egg::Compute::ComputeShader labelPotentialRecoverCS;

	com_ptr<ID3D12RootSignature> raymarchRootSig;
	com_ptr<ID3D12PipelineState> raymarchPso;

	Egg::ConstantBuffer<BccFrameCb>  frameCb;
	Egg::ConstantBuffer<TorusListCb> torusCb;

	com_ptr<ID3D12DescriptorHeap> imguiSrvHeap; // 1 slot, shader-visible, ImGui's font texture
	int renderMode = RenderMode_Labels; // driven by the ImGui dropdown, see BuildImGui()
	int initMethod = InitMethod_JFA;    // driven by the ImGui dropdown, see BuildImGui()
	int testShapeKind = TestShape_Torus; // driven by the ImGui dropdown, see BuildImGui() -- applied to torusCb by BuildShapeList(), called on every Reinitialize (not Continue)
	bool needsReinit = false;           // set by the ImGui "Reinitialize" button or the 'R' key, consumed in Render()
	bool needsContinue = false;         // set by the ImGui "Continue" button or the 'C' key, consumed in Render() -- see RunQuadricContinueOnly()
	bool gridInSrvState = false;        // false only before the very first RunInitPasses() call

	// -- multiphase label-potential filter (InitMethod_JfaFilter), see
	// labelPotential*CS.hlsl -- iteration count/relaxation rate are tunable
	// from the GUI so the effect can be judged interactively.
	int filterIterations = 6;      // driven by the ImGui slider, see BuildImGui()
	float filterRelaxRate = 0.5f;  // driven by the ImGui slider, see BuildImGui()

	// -- footvector visualization overlay (lines to, and spheres at, the
	// nearest-interface point every lattice point's footvector points to) --
	static constexpr uint FootVectorCount = 2 * GridRes * GridRes * GridRes; // A candidates then B candidates
	static constexpr uint FootVectorEntryStride = sizeof(float) * 8; // matches FootVectorEntry in FootVector.hlsli
	static constexpr float FootLineHalfWidthPx = 1.25f;
	static constexpr float FootPointRadiusPx   = 4.0f;

	Egg::Compute::ComputeShader footVectorBuildCS;
	com_ptr<ID3D12Resource> footVectorBuffer; // RWStructuredBuffer<FootVectorEntry>, root SRV/UAV (no heap descriptor needed)

	com_ptr<ID3D12RootSignature> footLineRootSig;
	com_ptr<ID3D12PipelineState> footLinePso;
	com_ptr<ID3D12RootSignature> footPointRootSig;
	com_ptr<ID3D12PipelineState> footPointPso;

	Egg::ConstantBuffer<FootVizParamsCb> footVizCb;

	bool showFootLines  = false; // driven by the ImGui checkbox, see BuildImGui()
	bool showFootPoints = false; // driven by the ImGui checkbox, see BuildImGui()
	Egg::Math::float3 footBoxMin = Egg::Math::float3(0.0f, 0.0f, 0.0f);
	Egg::Math::float3 footBoxMax = Egg::Math::float3(GridRes * CellSize, GridRes * CellSize, GridRes * CellSize);
	float footMaxLen = 8.0f;

	// -- quadric footpoint-field optimization (InitMethod_QuadricOptimization),
	// see QuadricFootField.hlsli. Operates on both sublattices together as one
	// flat [0, 2*LatticeNodeCount) node set (unlike JfaFilter, A isn't held
	// fixed here) -- StructuredBuffer<PackedNode4> hold each node's 4 label
	// slots (persistent normal + signed offset + curvature tensor per slot,
	// packed FP16, root SRV/UAV, no heap descriptors needed, same as
	// footVectorBuffer). --
	static constexpr uint QuadricNodeCount = 2 * GridRes * GridRes * GridRes;
	static constexpr uint PackedNodeStride = sizeof(uint) * 24; // 4 slots x uint2 x 3 -- matches PackedNode4 in QuadricFootField.hlsli
	static constexpr uint QuadricThreadGroups = (QuadricNodeCount + 63) / 64; // numthreads(64,1,1)

	Egg::Compute::ComputeShader quadricSeedCS;
	Egg::Compute::ComputeShader csOffsetGaussNewtonCS;
	Egg::Compute::ComputeShader csNormalRotationCS;
	Egg::Compute::ComputeShader csTransportTensorCS;
	Egg::Compute::ComputeShader csTensorGradientCS;
	Egg::Compute::ComputeShader quadricRecoverCS;

	com_ptr<ID3D12Resource> quadricOriginalNodes;
	com_ptr<ID3D12Resource> quadricCurrentNodes;
	com_ptr<ID3D12Resource> quadricFootUpdatedNodes;   // pass 1a's output (fresh offsets, old normals)
	com_ptr<ID3D12Resource> quadricNormalUpdatedNodes; // pass 1b's output (fresh normals too)
	com_ptr<ID3D12Resource> quadricTransportedNodes;
	com_ptr<ID3D12Resource> quadricOutputNodes;
	// true once quadricCurrentNodes/quadricOriginalNodes/sparseNodeSeeds have
	// been left in SRV state by a previous RunQuadricOptimizationInit() call
	// (false only before the very first one, when their creation state --
	// UAV -- already matches what quadricSeedCS/RunSparseLabelSeedInit need).
	bool quadricNodesNeedUavTransition = false;

	// -- sparse per-node label seeding (RunSparseLabelSeedInit): K independent
	// boundary-distance JFA sweeps (labelSnapshotCS/sparseLabelSeedCS, reusing
	// jfaStepCS/jfaFinalizeCS unmodified), harvested into a fixed 4-slot
	// record per node (sparseLabelHarvestCS) -- the SparseLabelCount nearest
	// labels' signed distance + seed, sorted ascending by |dist|, fixed
	// forever after this runs once. 4 is also used as K (the total label
	// count swept) purely because that matches BuildShapeList's current
	// background+3-tori convention -- the two uses of "4" are conceptually
	// distinct (total scene labels vs. slots per node) even though they share
	// a value today. See SparseLabelSeed.hlsli.
	static constexpr uint SparseLabelCount = 4;
	static constexpr uint SparseNodeSeedsStride = sizeof(uint) * 4 + sizeof(float) * 4 + sizeof(uint) * 4; // uint4+float4+uint4 -- matches SparseNodeSeeds

	Egg::Compute::ComputeShader labelSnapshotCS;
	Egg::Compute::ComputeShader sparseLabelSeedCS;
	Egg::Compute::ComputeShader sparseLabelClearCS;
	Egg::Compute::ComputeShader sparseLabelHarvestCS;

	com_ptr<ID3D12Resource> sparseNodeSeeds; // RWStructuredBuffer<SparseNodeSeeds>, 2*LatticeNodeCount entries, root UAV/SRV

	Egg::ConstantBuffer<OptimizationCB> quadricCb;

	int quadricIterations = 8; // outer iterations, driven by the ImGui slider, see BuildImGui()
	int quadricContinueIterations = 8; // for the "Continue" button/'C' key -- driven by the ImGui slider, see BuildImGui()

	// Jump-flood-style same-lattice neighbor reach for the current/next outer
	// iteration (see RunQuadricIterations) -- starts coarse, halves each
	// iteration down to 1, matching jfaStepCS.hlsl's own schedule. Only reset
	// to the coarse starting value by a full Reinitialize
	// (RunQuadricOptimizationInit); a Continue deliberately picks up wherever
	// this was left, so the coarse-to-fine sweep persists across repeated
	// Continue presses rather than restarting each time.
	uint quadricNeighborStep = 1;

	float quadricFootDataWeight   = 1.0f;
	float quadricFootSmoothWeight = 0.02f;
	float quadricFootStepDamping  = 0.25f;
	float quadricMinFootLength    = 0.001f;

	float quadricTensorDataWeight   = 0.2f;
	float quadricTensorSmoothWeight = 0.02f;
	// 0 by deliberate choice, not a TEMP flip: quadricSeedCS.hlsl now seeds
	// the exact analytic torus curvature (AnalyticTorusTensor) instead of
	// zero, and with TensorStep at 0 that seeded tensor is never touched
	// again (transport/project/clamp still apply, fitting does not) -- lets
	// the position/normal relaxation be tested against exact curvature,
	// isolating it from tensor-fitting error and linear (flat-plane)
	// approximation drift between neighbors.
	float quadricTensorStep         = 0.0f;
	float quadricMaxTensorFrobenius = 4.0f;

	float quadricResidualScale     = 0.5f;
	// Normal-alignment shaping is OFF by default (NormalPower<=0) -- distance
	// (quadricFootGateNear/Far below), not normal alignment, is now the
	// primary relevance filter. See NormalAlignmentWeight in QuadricFootField.hlsli.
	float quadricNormalPower       = 0.0f;
	float quadricMinNormalDot      = -1.0f;
	float quadricMinSystemDiagonal = 0.001f;

	// Also doubles as CSNormalRotation.hlsl's small-angle rotation clamp (see
	// QuadricOptimizationCb.hlsli).
	float quadricMaxFootStep = 0.25f;
	float quadricMaxResidual = 2.0f;
	bool  quadricUseResidualNormalization = false;

	// Same-node, cross-slot consistency weight -- see CrossLabelWeight in
	// QuadricOptimizationCb.hlsli.
	float quadricCrossLabelWeight = 1.0f;

	// Soft footpoint-distance gate -- see FootDistanceWeight in
	// QuadricFootField.hlsli. Defaults matching the "2-3 grid cells" scale.
	float quadricFootGateNear = 2.0f * CellSize;
	float quadricFootGateFar  = 3.0f * CellSize;

	// -- foot-quadric wireframe visualization: for a stride-selected subset
	// of nodes, draws the actual (possibly non-paraboloid) quadric surface
	// each node's footvector+curvature tensor define, clipped to a radius
	// around the footpoint -- see quadricPatchBuildCS.hlsl. Only meaningful
	// right after a QuadricOptimization run (quadricCurrentNodes is the only
	// place the curvature tensor survives; quadricRecoverCS discards it when
	// writing back to the grid), so quadricDataValid gates the GUI/draw
	// entirely, independent of whatever initMethod is currently selected in
	// the dropdown. --
	static constexpr uint QuadricPatchRingSamples = 16; // must match quadricPatchBuildCS.hlsl's RingSamples
	// 2 open "normal section" curves (RingSamples-1 segments each) + 1 closed
	// "tangent ring" curve (RingSamples segments) -- must match
	// quadricPatchBuildCS.hlsl's SegmentsPerNode.
	static constexpr uint QuadricPatchSegmentsPerNode = 2 * (QuadricPatchRingSamples - 1) + QuadricPatchRingSamples;
	// Fixed buffer capacity (rather than a resizable one) -- the stride
	// slider's minimum is clamped so the selected node count can never
	// exceed this, regardless of GridRes.
	static constexpr uint MaxQuadricPatches = 2048;
	static constexpr uint QuadricPatchMinStride = (QuadricNodeCount + MaxQuadricPatches - 1) / MaxQuadricPatches;

	Egg::Compute::ComputeShader quadricPatchBuildCS;
	com_ptr<ID3D12RootSignature> quadricPatchLineRootSig;
	com_ptr<ID3D12PipelineState> quadricPatchLinePso;
	com_ptr<ID3D12Resource> quadricPatchSegments;

	bool quadricDataValid = false; // true only right after RunQuadricOptimizationInit(), see RunInitPasses()
	bool showFootQuadrics = false; // driven by the ImGui checkbox, see BuildImGui()
	int quadricPatchStride = 512;
	int quadricPatchOffset = 0;
	float quadricPatchRadius = 2.0f * CellSize;

	// Test shape: labels/positions/axes/radii, edit freely for testing.
	// Rebuilds torusCb.data entirely from testShapeKind -- called once at
	// load time and again on every Reinitialize (not Continue, see
	// RunInitPasses), so switching "Test Shape" in the GUI takes effect on
	// the next Reinitialize.
	void BuildShapeList() {
		using namespace Egg::Math;
		float3 center(GridRes * CellSize * 0.5f, GridRes * CellSize * 0.5f, GridRes * CellSize * 0.5f);

		torusCb.data.ShapeKind = (uint)testShapeKind;

		if (testShapeKind == TestShape_Ellipsoid) {
			// Single axis-aligned ellipsoid, roughly comparable overall
			// extent to the torus test shape (major+minor radius ~19).
			// TorusDesc.axis is reused to hold semi-axis radii (rx,ry,rz)
			// directly rather than a rotation axis; majorRadius/minorRadius
			// are unused for this shape kind -- see TorusSdf.hlsli's
			// SdEllipsoid / quadricSeedCS.hlsl's AnalyticEllipsoidTensor.
			torusCb.data.nTorii = 1;
			torusCb.data.torii[0].center      = center;
			torusCb.data.torii[0].axis        = float3(10.0f, 7.0f, 5.0f);
			torusCb.data.torii[0].majorRadius = 0.0f;
			torusCb.data.torii[0].minorRadius = 0.0f;
			torusCb.data.torii[0].label       = 1;
			return;
		}

		struct { float3 offset, axis; float major, minor; uint label; } list[] = {
			{ float3(0, 0, 0),   float3(0, 1, 0), 14.0f, 5.0f, 1 },
			// Other two tori disabled to isolate the layering/divergence bug on
			// a single, junction-free torus. Restore to test with multiple tori again.
			//{ float3(0, 0, 0),   float3(1, 0, 0), 10.0f, 4.0f, 2 },
			//{ float3(0, -9, 0),  float3(0, 0, 1),  8.0f, 3.0f, 3 },
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

	void CreateGridTextures() {
		D3D12_RESOURCE_DESC texDesc = {};
		texDesc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
		texDesc.Width            = GridRes;
		texDesc.Height           = GridRes;
		texDesc.DepthOrArraySize = (UINT16)GridRes;
		texDesc.MipLevels        = 1;
		texDesc.Format           = DXGI_FORMAT_R32G32B32A32_UINT;
		texDesc.SampleDesc.Count = 1;
		texDesc.Layout           = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		texDesc.Flags            = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		const wchar_t* names[SlotCount] = { L"gridA0", L"gridA1", L"gridB0", L"gridB1", L"gridAFoot", L"gridBFoot" };

		uint dhIncrSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		for (uint i = 0; i < SlotCount; i++) {
			DX_API("create BCC grid texture")
				device->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
					D3D12_HEAP_FLAG_NONE,
					&texDesc,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
					nullptr,
					IID_PPV_ARGS(gridTex[i].ReleaseAndGetAddressOf()));
			gridTex[i]->SetName(names[i]);

			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
			uavDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
			uavDesc.Texture3D.MipSlice = 0;
			uavDesc.Texture3D.FirstWSlice = 0;
			uavDesc.Texture3D.WSize = GridRes;
			CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle(uavHeap->GetCPUDescriptorHandleForHeapStart(), i, dhIncrSize);
			device->CreateUnorderedAccessView(gridTex[i].Get(), nullptr, &uavDesc, uavHandle);

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Texture3D.MipLevels = 1;
			CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(srvHeap->GetCPUDescriptorHandleForHeapStart(), i, dhIncrSize);
			device->CreateShaderResourceView(gridTex[i].Get(), &srvDesc, srvHandle);
		}
	}

	static D3D12_RESOURCE_BARRIER GridUavBarrier(ID3D12Resource* res) {
		D3D12_RESOURCE_BARRIER b = {};
		b.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		b.UAV.pResource = res;
		return b;
	}

	void BarrierAllGrids(com_ptr<ID3D12GraphicsCommandList>& cmd) {
		D3D12_RESOURCE_BARRIER b[SlotCount];
		for (uint i = 0; i < SlotCount; i++) b[i] = GridUavBarrier(gridTex[i].Get());
		cmd->ResourceBarrier(SlotCount, b);
	}

	void RunJfaInit(com_ptr<ID3D12GraphicsCommandList>& cmd, D3D12_GPU_DESCRIPTOR_HANDLE uavGpu) {
		// -- torus rasterization: A then B --
		cmd->SetComputeRootSignature(torusInitCS.rootSig.Get());
		cmd->SetPipelineState(torusInitCS.pso.Get());
		cmd->SetComputeRootDescriptorTable(1, uavGpu);
		cmd->SetComputeRootConstantBufferView(2, torusCb.GetGPUVirtualAddress());
		for (uint domainIsB = 0; domainIsB < 2; domainIsB++) {
			cmd->SetComputeRoot32BitConstant(0, domainIsB, 0);
			cmd->Dispatch(ThreadGroups, ThreadGroups, ThreadGroups);
		}
		BarrierAllGrids(cmd);

		// -- seed interfaces: A then B --
		cmd->SetComputeRootSignature(jfaSeedCS.rootSig.Get());
		cmd->SetPipelineState(jfaSeedCS.pso.Get());
		cmd->SetComputeRootDescriptorTable(1, uavGpu);
		for (uint domainIsB = 0; domainIsB < 2; domainIsB++) {
			cmd->SetComputeRoot32BitConstant(0, domainIsB, 0);
			cmd->Dispatch(ThreadGroups, ThreadGroups, ThreadGroups);
		}
		BarrierAllGrids(cmd);

		// -- jump flood: step sizes N/2 .. 1, plus two "+1" refinement passes --
		cmd->SetComputeRootSignature(jfaStepCS.rootSig.Get());
		cmd->SetPipelineState(jfaStepCS.pso.Get());
		cmd->SetComputeRootDescriptorTable(1, uavGpu);

		std::vector<uint> stepSizes;
		for (uint s = GridRes / 2; s >= 1; s /= 2) stepSizes.push_back(s);
		stepSizes.push_back(1);
		stepSizes.push_back(1);

		uint pingIndex = 0;
		for (uint stepSize : stepSizes) {
			for (uint domainIsB = 0; domainIsB < 2; domainIsB++) {
				cmd->SetComputeRoot32BitConstant(0, stepSize, 0);
				cmd->SetComputeRoot32BitConstant(0, pingIndex, 1);
				cmd->SetComputeRoot32BitConstant(0, domainIsB, 2);
				cmd->Dispatch(ThreadGroups, ThreadGroups, ThreadGroups);
			}
			BarrierAllGrids(cmd);
			pingIndex = 1 - pingIndex;
		}

		// -- finalize: guarantee the result lives in A0/B0 --
		cmd->SetComputeRootSignature(jfaFinalizeCS.rootSig.Get());
		cmd->SetPipelineState(jfaFinalizeCS.pso.Get());
		cmd->SetComputeRootDescriptorTable(1, uavGpu);
		cmd->SetComputeRoot32BitConstant(0, pingIndex, 0);
		cmd->Dispatch(ThreadGroups, ThreadGroups, ThreadGroups);
		BarrierAllGrids(cmd);
	}

	void RunAnalyticInit(com_ptr<ID3D12GraphicsCommandList>& cmd, D3D12_GPU_DESCRIPTOR_HANDLE uavGpu) {
		cmd->SetComputeRootSignature(analyticInitCS.rootSig.Get());
		cmd->SetPipelineState(analyticInitCS.pso.Get());
		cmd->SetComputeRootDescriptorTable(1, uavGpu);
		cmd->SetComputeRootConstantBufferView(2, torusCb.GetGPUVirtualAddress());
		for (uint domainIsB = 0; domainIsB < 2; domainIsB++) {
			cmd->SetComputeRoot32BitConstant(0, domainIsB, 0);
			cmd->Dispatch(ThreadGroups, ThreadGroups, ThreadGroups);
		}
		BarrierAllGrids(cmd);
	}

	// Runs ordinary JFA, then refines B0's result with the multiphase
	// label-potential filter: seed potentials from B0's JFA texel, relax them
	// for filterIterations steps (ping-ponging the repurposed A1/B1 scratch --
	// JFA's own ping-pong is done by this point, and A0 stays fixed/read-only
	// throughout as the anchor data), then recover the filtered result back
	// into B0's existing texel format + gBFoot. See labelPotential*CS.hlsl.
	void RunJfaFilterInit(com_ptr<ID3D12GraphicsCommandList>& cmd, D3D12_GPU_DESCRIPTOR_HANDLE uavGpu) {
		RunJfaInit(cmd, uavGpu);

		cmd->SetComputeRootSignature(labelPotentialInitCS.rootSig.Get());
		cmd->SetPipelineState(labelPotentialInitCS.pso.Get());
		cmd->SetComputeRootDescriptorTable(0, uavGpu);
		cmd->Dispatch(ThreadGroups, ThreadGroups, ThreadGroups);
		BarrierAllGrids(cmd);

		float relaxRate = filterRelaxRate;
		uint relaxBits = *reinterpret_cast<uint*>(&relaxRate);

		cmd->SetComputeRootSignature(labelPotentialDiffuseCS.rootSig.Get());
		cmd->SetPipelineState(labelPotentialDiffuseCS.pso.Get());
		cmd->SetComputeRootDescriptorTable(1, uavGpu);

		// pingIndex==0 means "current potentials are in B1" (where the init
		// pass just wrote them), matching labelPotentialDiffuseCS.hlsl's
		// convention; after the loop its value directly tells the recover
		// pass which buffer to read (0=B1, 1=A1), same idea as
		// jfaFinalizeCS's finalPingIndex.
		uint pingIndex = 0;
		int iterations = filterIterations > 0 ? filterIterations : 0;
		for (int i = 0; i < iterations; i++) {
			cmd->SetComputeRoot32BitConstant(0, pingIndex, 0);
			cmd->SetComputeRoot32BitConstant(0, relaxBits, 1);
			cmd->Dispatch(ThreadGroups, ThreadGroups, ThreadGroups);
			BarrierAllGrids(cmd);
			pingIndex = 1 - pingIndex;
		}

		cmd->SetComputeRootSignature(labelPotentialRecoverCS.rootSig.Get());
		cmd->SetPipelineState(labelPotentialRecoverCS.pso.Get());
		cmd->SetComputeRootDescriptorTable(1, uavGpu);
		cmd->SetComputeRoot32BitConstant(0, pingIndex, 0);
		cmd->Dispatch(ThreadGroups, ThreadGroups, ThreadGroups);
		BarrierAllGrids(cmd);
	}

	// Runs ordinary JFA, converts its A0/B0 result into the flat PackedNode
	// representation (quadricSeedCS -- footvector from the existing seed
	// reconstruction, curvature tensor zeroed), relaxes it for
	// quadricIterations outer iterations (3-pass Gauss-Newton/tensor-transport/
	// tensor-gradient scheme, ping-ponging quadricCurrentNodes/
	// quadricOutputNodes), then converts the final state back into A0/B0 +
	// gAFoot/gBFoot (quadricRecoverCS) -- same "convert back to the existing
	// format" strategy RunJfaFilterInit uses, so raymarchPS.hlsl/
	// footVectorBuildCS.hlsl need no changes. See QuadricFootField.hlsli.
	// Runs K (SparseLabelCount) independent unsigned-distance JFA sweeps, one
	// per label, and harvests each node's SparseLabelCount nearest labels'
	// (label, distance, seed) into sparseNodeSeeds -- fixed forever, never
	// revisited by anything downstream. Must run before RunJfaInit's ordinary
	// jfaSeedCS/jfaStepCS overwrite A0/B0 with the boundary-JFA result this
	// doesn't need; RunJfaInit re-rasterizes A0/B0 from scratch as its own
	// first step regardless, so this doesn't need to restore them itself.
	void RunSparseLabelSeedInit(com_ptr<ID3D12GraphicsCommandList>& cmd, D3D12_GPU_DESCRIPTOR_HANDLE uavGpu) {
		// -- ground-truth label rasterization (A then B), then a persistent
		// snapshot into AFoot/BFoot -- A0/B0 get overwritten repeatedly below,
		// once per label's own JFA propagation. --
		cmd->SetComputeRootSignature(torusInitCS.rootSig.Get());
		cmd->SetPipelineState(torusInitCS.pso.Get());
		cmd->SetComputeRootDescriptorTable(1, uavGpu);
		cmd->SetComputeRootConstantBufferView(2, torusCb.GetGPUVirtualAddress());
		for (uint domainIsB = 0; domainIsB < 2; domainIsB++) {
			cmd->SetComputeRoot32BitConstant(0, domainIsB, 0);
			cmd->Dispatch(ThreadGroups, ThreadGroups, ThreadGroups);
		}
		BarrierAllGrids(cmd);

		cmd->SetComputeRootSignature(labelSnapshotCS.rootSig.Get());
		cmd->SetPipelineState(labelSnapshotCS.pso.Get());
		cmd->SetComputeRootDescriptorTable(0, uavGpu);
		cmd->Dispatch(ThreadGroups, ThreadGroups, ThreadGroups);
		BarrierAllGrids(cmd);

		cmd->SetComputeRootSignature(sparseLabelClearCS.rootSig.Get());
		cmd->SetPipelineState(sparseLabelClearCS.pso.Get());
		cmd->SetComputeRootConstantBufferView(0, quadricCb.GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(1, sparseNodeSeeds->GetGPUVirtualAddress());
		cmd->Dispatch(QuadricThreadGroups, 1, 1);

		std::vector<uint> stepSizes;
		for (uint s = GridRes / 2; s >= 1; s /= 2) stepSizes.push_back(s);
		stepSizes.push_back(1);
		stepSizes.push_back(1);

		for (uint label = 0; label < SparseLabelCount; label++) {
			// Seed: distance 0 at every voxel whose ground-truth label ==
			// this one, unseeded (SENTINEL_LABEL) everywhere else.
			cmd->SetComputeRootSignature(sparseLabelSeedCS.rootSig.Get());
			cmd->SetPipelineState(sparseLabelSeedCS.pso.Get());
			cmd->SetComputeRootDescriptorTable(1, uavGpu);
			for (uint domainIsB = 0; domainIsB < 2; domainIsB++) {
				uint consts[2] = { domainIsB, label };
				cmd->SetComputeRoot32BitConstants(0, 2, consts, 0);
				cmd->Dispatch(ThreadGroups, ThreadGroups, ThreadGroups);
			}
			BarrierAllGrids(cmd);

			// Propagate (reusing jfaStepCS/jfaFinalizeCS completely unmodified
			// -- this label's seed texel layout matches jfaSeedCS's exactly).
			cmd->SetComputeRootSignature(jfaStepCS.rootSig.Get());
			cmd->SetPipelineState(jfaStepCS.pso.Get());
			cmd->SetComputeRootDescriptorTable(1, uavGpu);
			uint pingIndex = 0;
			for (uint s : stepSizes) {
				for (uint domainIsB = 0; domainIsB < 2; domainIsB++) {
					uint consts[3] = { s, pingIndex, domainIsB };
					cmd->SetComputeRoot32BitConstants(0, 3, consts, 0);
					cmd->Dispatch(ThreadGroups, ThreadGroups, ThreadGroups);
				}
				BarrierAllGrids(cmd);
				pingIndex = 1 - pingIndex;
			}

			cmd->SetComputeRootSignature(jfaFinalizeCS.rootSig.Get());
			cmd->SetPipelineState(jfaFinalizeCS.pso.Get());
			cmd->SetComputeRootDescriptorTable(1, uavGpu);
			cmd->SetComputeRoot32BitConstant(0, pingIndex, 0);
			cmd->Dispatch(ThreadGroups, ThreadGroups, ThreadGroups);
			BarrierAllGrids(cmd);

			// Harvest this label's converged distance into the 4-slot record.
			cmd->SetComputeRootSignature(sparseLabelHarvestCS.rootSig.Get());
			cmd->SetPipelineState(sparseLabelHarvestCS.pso.Get());
			cmd->SetComputeRootConstantBufferView(0, quadricCb.GetGPUVirtualAddress());
			cmd->SetComputeRootDescriptorTable(2, uavGpu);
			cmd->SetComputeRootUnorderedAccessView(3, sparseNodeSeeds->GetGPUVirtualAddress());
			for (uint domainIsB = 0; domainIsB < 2; domainIsB++) {
				uint consts[2] = { domainIsB, label };
				cmd->SetComputeRoot32BitConstants(1, 2, consts, 0);
				cmd->Dispatch(ThreadGroups, ThreadGroups, ThreadGroups);
			}
			BarrierAllGrids(cmd);
		}
	}

	// Runs the K-JFA sparse label seeding, converts it into the flat 4-slot
	// PackedNode4 representation (quadricSeedCS -- per slot, a signed offset
	// and an honestly-oriented persistent normal from the boundary-distance
	// seed, curvature tensor zeroed), relaxes it for quadricIterations outer
	// iterations (4-pass scheme per iteration: offset Gauss-Newton, normal
	// rotation, tensor transport, tensor gradient -- ping-ponging
	// quadricCurrentNodes/quadricOutputNodes), then converts the final
	// dominant-slot state back into A0/B0 + gAFoot/gBFoot (quadricRecoverCS)
	// -- same "convert back to the existing format" strategy RunJfaFilterInit
	// uses, so raymarchPS.hlsl/footVectorBuildCS.hlsl need no changes. See
	// QuadricFootField.hlsli / SparseLabelSeed.hlsli.
	void UploadQuadricCb() {
		quadricCb.data.GridSize = Egg::Math::uint3(GridRes, GridRes, GridRes);
		quadricCb.data.LatticeNodeCount = GridRes * GridRes * GridRes;
		quadricCb.data.GridOrigin = Egg::Math::float3(0.0f, 0.0f, 0.0f);
		quadricCb.data.NodeCellSize = CellSize;
		quadricCb.data.FootDataWeight = quadricFootDataWeight;
		quadricCb.data.FootSmoothWeight = quadricFootSmoothWeight;
		quadricCb.data.FootStepDamping = quadricFootStepDamping;
		quadricCb.data.MinFootLength = quadricMinFootLength;
		quadricCb.data.TensorDataWeight = quadricTensorDataWeight;
		quadricCb.data.TensorSmoothWeight = quadricTensorSmoothWeight;
		quadricCb.data.TensorStep = quadricTensorStep;
		quadricCb.data.MaxTensorFrobenius = quadricMaxTensorFrobenius;
		quadricCb.data.ResidualScale = quadricResidualScale;
		quadricCb.data.NormalPower = quadricNormalPower;
		quadricCb.data.MinNormalDot = quadricMinNormalDot;
		quadricCb.data.MinSystemDiagonal = quadricMinSystemDiagonal;
		quadricCb.data.MaxFootStep = quadricMaxFootStep;
		quadricCb.data.MaxResidual = quadricMaxResidual;
		quadricCb.data.UseResidualNormalization = quadricUseResidualNormalization ? 1u : 0u;
		quadricCb.data.CrossLabelWeight = quadricCrossLabelWeight;
		quadricCb.data.FootGateNear = quadricFootGateNear;
		quadricCb.data.FootGateFar = quadricFootGateFar;
		quadricCb.Upload();
	}

	// Runs `count` more outer iterations (4-pass scheme: offset Gauss-Newton,
	// normal rotation, tensor transport, tensor gradient -- ping-ponging
	// quadricCurrentNodes/quadricOutputNodes) on whatever quadricCurrentNodes/
	// quadricOriginalNodes/sparseNodeSeeds currently hold. Callable standalone
	// (RunQuadricContinueOnly, no re-seeding) or right after seeding
	// (RunQuadricOptimizationInit) -- the buffer states this assumes at entry
	// (CurrentNodes/OriginalNodes/Seeds readable as SRV, the four scratch
	// buffers UAV-writable) are exactly what both callers, and the end of
	// this loop itself, always leave behind.
	void RunQuadricIterations(com_ptr<ID3D12GraphicsCommandList>& cmd, int count) {
		int iterations = count > 0 ? count : 0;
		for (int i = 0; i < iterations; i++) {
			// Jump-flood-style same-lattice neighbor reach for this iteration
			// -- see quadricNeighborStep's declaration. Persists across calls
			// (both a full Reinitialize and every subsequent Continue), only
			// ever reset back to the coarse starting value in
			// RunQuadricOptimizationInit.
			uint neighborStep = quadricNeighborStep;
			quadricNeighborStep = (quadricNeighborStep > 1) ? quadricNeighborStep / 2 : 1;

			// Pass 1a: t0=CurrentNodes(SRV), t1=OriginalNodes(SRV), t2=Seeds(SRV), u0=FootUpdatedNodes(UAV)
			cmd->SetComputeRootSignature(csOffsetGaussNewtonCS.rootSig.Get());
			cmd->SetPipelineState(csOffsetGaussNewtonCS.pso.Get());
			cmd->SetComputeRootConstantBufferView(0, quadricCb.GetGPUVirtualAddress());
			cmd->SetComputeRoot32BitConstant(1, neighborStep, 0);
			cmd->SetComputeRootShaderResourceView(2, quadricCurrentNodes->GetGPUVirtualAddress());
			cmd->SetComputeRootShaderResourceView(3, quadricOriginalNodes->GetGPUVirtualAddress());
			cmd->SetComputeRootShaderResourceView(4, sparseNodeSeeds->GetGPUVirtualAddress());
			cmd->SetComputeRootUnorderedAccessView(5, quadricFootUpdatedNodes->GetGPUVirtualAddress());
			cmd->Dispatch(QuadricThreadGroups, 1, 1);

			// FootUpdatedNodes becomes an SRV input for pass 1b.
			cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(quadricFootUpdatedNodes.Get(),
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

			// Pass 1b: t0=FootUpdatedNodes(SRV), t2=Seeds(SRV), u1=NormalUpdatedNodes(UAV)
			cmd->SetComputeRootSignature(csNormalRotationCS.rootSig.Get());
			cmd->SetPipelineState(csNormalRotationCS.pso.Get());
			cmd->SetComputeRootConstantBufferView(0, quadricCb.GetGPUVirtualAddress());
			cmd->SetComputeRoot32BitConstant(1, neighborStep, 0);
			cmd->SetComputeRootShaderResourceView(2, quadricFootUpdatedNodes->GetGPUVirtualAddress());
			cmd->SetComputeRootShaderResourceView(3, sparseNodeSeeds->GetGPUVirtualAddress());
			cmd->SetComputeRootUnorderedAccessView(4, quadricNormalUpdatedNodes->GetGPUVirtualAddress());
			cmd->Dispatch(QuadricThreadGroups, 1, 1);

			// NormalUpdatedNodes becomes an SRV input for pass 2; FootUpdatedNodes
			// goes back to UAV-writable, ready for the next iteration.
			D3D12_RESOURCE_BARRIER afterRotation[] = {
				CD3DX12_RESOURCE_BARRIER::Transition(quadricNormalUpdatedNodes.Get(),
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
				CD3DX12_RESOURCE_BARRIER::Transition(quadricFootUpdatedNodes.Get(),
					D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
			};
			cmd->ResourceBarrier(_countof(afterRotation), afterRotation);

			// Pass 2: t0=CurrentNodes(SRV, pre-iteration/old normals), t2=NormalUpdatedNodes(SRV), u1=TransportedNodes(UAV)
			cmd->SetComputeRootSignature(csTransportTensorCS.rootSig.Get());
			cmd->SetPipelineState(csTransportTensorCS.pso.Get());
			cmd->SetComputeRootConstantBufferView(0, quadricCb.GetGPUVirtualAddress());
			cmd->SetComputeRootShaderResourceView(1, quadricCurrentNodes->GetGPUVirtualAddress());
			cmd->SetComputeRootShaderResourceView(2, quadricNormalUpdatedNodes->GetGPUVirtualAddress());
			cmd->SetComputeRootUnorderedAccessView(3, quadricTransportedNodes->GetGPUVirtualAddress());
			cmd->Dispatch(QuadricThreadGroups, 1, 1);

			// TransportedNodes becomes an SRV input for pass 3; NormalUpdatedNodes
			// goes back to UAV-writable, ready for the next iteration.
			D3D12_RESOURCE_BARRIER afterTransport[] = {
				CD3DX12_RESOURCE_BARRIER::Transition(quadricTransportedNodes.Get(),
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
				CD3DX12_RESOURCE_BARRIER::Transition(quadricNormalUpdatedNodes.Get(),
					D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
			};
			cmd->ResourceBarrier(_countof(afterTransport), afterTransport);

			// Pass 3: t1=OriginalNodes(SRV), t2=Seeds(SRV), t3=TransportedNodes(SRV), u2=OutputNodes(UAV)
			cmd->SetComputeRootSignature(csTensorGradientCS.rootSig.Get());
			cmd->SetPipelineState(csTensorGradientCS.pso.Get());
			cmd->SetComputeRootConstantBufferView(0, quadricCb.GetGPUVirtualAddress());
			cmd->SetComputeRoot32BitConstant(1, neighborStep, 0);
			cmd->SetComputeRootShaderResourceView(2, quadricOriginalNodes->GetGPUVirtualAddress());
			cmd->SetComputeRootShaderResourceView(3, sparseNodeSeeds->GetGPUVirtualAddress());
			cmd->SetComputeRootShaderResourceView(4, quadricTransportedNodes->GetGPUVirtualAddress());
			cmd->SetComputeRootUnorderedAccessView(5, quadricOutputNodes->GetGPUVirtualAddress());
			cmd->Dispatch(QuadricThreadGroups, 1, 1);

			// Prepare TransportedNodes for the next outer iteration, and swap
			// OutputNodes/CurrentNodes (SRV/UAV roles) the same way the old
			// single-pass scheme did -- then the C++-side swap makes that
			// official ("swap OutputNodes into CurrentNodes").
			D3D12_RESOURCE_BARRIER endOfIter[] = {
				CD3DX12_RESOURCE_BARRIER::Transition(quadricTransportedNodes.Get(),
					D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
				CD3DX12_RESOURCE_BARRIER::Transition(quadricOutputNodes.Get(),
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
				CD3DX12_RESOURCE_BARRIER::Transition(quadricCurrentNodes.Get(),
					D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
			};
			cmd->ResourceBarrier(_countof(endOfIter), endOfIter);

			std::swap(quadricCurrentNodes, quadricOutputNodes);
		}
	}

	// Converts the current CurrentNodes' dominant slot per node back into
	// A0/B0 + gAFoot/gBFoot. Shared by both the full re-seed path
	// (RunQuadricOptimizationInit) and the no-reseed continue path
	// (RunQuadricContinueOnly).
	void RunQuadricRecover(com_ptr<ID3D12GraphicsCommandList>& cmd, D3D12_GPU_DESCRIPTOR_HANDLE uavGpu) {
		cmd->SetComputeRootSignature(quadricRecoverCS.rootSig.Get());
		cmd->SetPipelineState(quadricRecoverCS.pso.Get());
		cmd->SetComputeRootConstantBufferView(0, quadricCb.GetGPUVirtualAddress());
		cmd->SetComputeRootDescriptorTable(1, uavGpu);
		cmd->SetComputeRootShaderResourceView(2, quadricCurrentNodes->GetGPUVirtualAddress());
		cmd->SetComputeRootShaderResourceView(3, sparseNodeSeeds->GetGPUVirtualAddress());
		cmd->Dispatch(QuadricThreadGroups, 1, 1);
	}

	// Runs quadricContinueIterations more outer iterations on top of whatever
	// a previous RunQuadricOptimizationInit() (or an earlier continue) left
	// in quadricCurrentNodes/quadricOriginalNodes/sparseNodeSeeds -- no
	// re-seeding, no K-JFA re-run -- then re-runs the recover pass so the
	// visible grid reflects the additional relaxation. Only meaningful when
	// quadricDataValid is already true (see the "Continue" button/'C' key in
	// BuildImGui()/ProcessMessage()). All five node buffers are already left
	// in exactly the state RunQuadricIterations expects by the end of a
	// previous run, so no extra barriers are needed here beyond what that
	// loop and the recover pass do themselves.
	void RunQuadricContinueOnly(com_ptr<ID3D12GraphicsCommandList>& cmd, D3D12_GPU_DESCRIPTOR_HANDLE uavGpu) {
		UploadQuadricCb(); // pick up any slider changes made since the last run
		RunQuadricIterations(cmd, quadricContinueIterations);
		RunQuadricRecover(cmd, uavGpu);
		quadricDataValid = true;
	}

	void RunQuadricOptimizationInit(com_ptr<ID3D12GraphicsCommandList>& cmd, D3D12_GPU_DESCRIPTOR_HANDLE uavGpu) {
		UploadQuadricCb();
		quadricNeighborStep = (GridRes / 4 > 1) ? (GridRes / 4) : 1u; // fresh coarse-to-fine sweep on every full reseed

		if (quadricNodesNeedUavTransition) {
			D3D12_RESOURCE_BARRIER toUav[] = {
				CD3DX12_RESOURCE_BARRIER::Transition(quadricCurrentNodes.Get(),
					D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
				CD3DX12_RESOURCE_BARRIER::Transition(quadricOriginalNodes.Get(),
					D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
				CD3DX12_RESOURCE_BARRIER::Transition(sparseNodeSeeds.Get(),
					D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
			};
			cmd->ResourceBarrier(_countof(toUav), toUav);
		}

		RunSparseLabelSeedInit(cmd, uavGpu);
		RunJfaInit(cmd, uavGpu);

		// sparseNodeSeeds is a read-only input for the rest of this init (and
		// for quadricPatchBuildCS's wireframe overlay afterward).
		cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(sparseNodeSeeds.Get(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

		// -- seed: convert sparseNodeSeeds into PackedNode4 form. No grid
		// textures touched at all from here through the end of the outer
		// iteration loop -- this whole relaxation is purely buffer-to-buffer. --
		{
			cmd->SetComputeRootSignature(quadricSeedCS.rootSig.Get());
			cmd->SetPipelineState(quadricSeedCS.pso.Get());
			cmd->SetComputeRootConstantBufferView(0, quadricCb.GetGPUVirtualAddress());
			cmd->SetComputeRootShaderResourceView(1, sparseNodeSeeds->GetGPUVirtualAddress());
			cmd->SetComputeRootUnorderedAccessView(2, quadricOriginalNodes->GetGPUVirtualAddress());
			cmd->SetComputeRootUnorderedAccessView(3, quadricCurrentNodes->GetGPUVirtualAddress());
			cmd->SetComputeRootConstantBufferView(4, torusCb.GetGPUVirtualAddress());
			cmd->Dispatch(QuadricThreadGroups, 1, 1);
		}

		// Both are now read-only inputs for the rest of this init.
		{
			D3D12_RESOURCE_BARRIER toSrv[] = {
				CD3DX12_RESOURCE_BARRIER::Transition(quadricOriginalNodes.Get(),
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
				CD3DX12_RESOURCE_BARRIER::Transition(quadricCurrentNodes.Get(),
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
			};
			cmd->ResourceBarrier(_countof(toSrv), toSrv);
		}

		RunQuadricIterations(cmd, quadricIterations);
		RunQuadricRecover(cmd, uavGpu);

		quadricNodesNeedUavTransition = true; // quadricCurrentNodes/quadricOriginalNodes/sparseNodeSeeds are left in SRV state
		quadricDataValid = true; // quadricCurrentNodes now holds a real footvector+curvature-tensor field for the patch overlay
	}

	// (Re)populates A0/B0 using whichever method initMethod currently selects.
	// Runs once at load time, and again whenever the "Reinitialize" button is
	// pressed -- in which case the grids are already in their post-init SRV
	// state and need transitioning back to UAV first. The per-frame compute
	// queue is left free for future footvector-filtering passes regardless.
	// continueOnly=true skips re-seeding entirely (no torus rasterization, no
	// K-JFA re-run, no quadricSeedCS) and just runs more outer iterations on
	// top of whatever quadricCurrentNodes/quadricOriginalNodes/sparseNodeSeeds
	// already hold -- see RunQuadricContinueOnly(). Only meaningful right
	// after a QuadricOptimization run (quadricDataValid already true); the
	// "Continue" button/'C' key are themselves gated on that.
	void RunInitPasses(bool continueOnly = false) {
		// Only RunQuadricOptimizationInit()/RunQuadricContinueOnly() set this
		// to true -- any other init method leaves quadricCurrentNodes stale
		// (or never written), so the foot-quadric overlay must hide/disable
		// regardless of whatever initMethod is currently selected in the
		// dropdown. A continue never needs to hide it first, since it can
		// only run when it's already true.
		if (!continueOnly) {
			quadricDataValid = false;
			BuildShapeList(); // picks up the current "Test Shape" GUI choice
			torusCb.Upload();
		}

		DX_API("reset upload allocator") uploadAllocator->Reset();
		DX_API("reset upload command list") uploadCommandList->Reset(uploadAllocator.Get(), nullptr);
		auto& cmd = uploadCommandList;

		ID3D12DescriptorHeap* heaps[] = { uavHeap.Get() };
		cmd->SetDescriptorHeaps(1, heaps);
		D3D12_GPU_DESCRIPTOR_HANDLE uavGpu = uavHeap->GetGPUDescriptorHandleForHeapStart();

		if (gridInSrvState) {
			D3D12_RESOURCE_BARRIER toUav[SlotCount];
			for (uint i = 0; i < SlotCount; i++) {
				toUav[i] = CD3DX12_RESOURCE_BARRIER::Transition(gridTex[i].Get(),
					D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			}
			cmd->ResourceBarrier(SlotCount, toUav);

			cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(footVectorBuffer.Get(),
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
		}

		if (continueOnly)
			RunQuadricContinueOnly(cmd, uavGpu);
		else if (initMethod == InitMethod_Analytic)
			RunAnalyticInit(cmd, uavGpu);
		else if (initMethod == InitMethod_JfaFilter)
			RunJfaFilterInit(cmd, uavGpu);
		else if (initMethod == InitMethod_QuadricOptimization)
			RunQuadricOptimizationInit(cmd, uavGpu);
		else
			RunJfaInit(cmd, uavGpu);

		// The raymarch PS reads A0/B0 (and, harmlessly, A1/B1) as SRVs every
		// frame from here on -- move all four out of UAV state for good.
		{
			D3D12_RESOURCE_BARRIER toSrv[SlotCount];
			for (uint i = 0; i < SlotCount; i++) {
				toSrv[i] = CD3DX12_RESOURCE_BARRIER::Transition(gridTex[i].Get(),
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
					D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			}
			cmd->ResourceBarrier(SlotCount, toSrv);
		}
		gridInSrvState = true;

		// -- rebuild the footvector visualization buffer from the now-converged
		// grids; reads them as SRVs, so bind srvHeap instead of uavHeap --
		{
			ID3D12DescriptorHeap* srvHeaps[] = { srvHeap.Get() };
			cmd->SetDescriptorHeaps(1, srvHeaps);
			cmd->SetComputeRootSignature(footVectorBuildCS.rootSig.Get());
			cmd->SetPipelineState(footVectorBuildCS.pso.Get());
			bool useExactFoot = initMethod == InitMethod_Analytic || initMethod == InitMethod_JfaFilter
				|| initMethod == InitMethod_QuadricOptimization;
			cmd->SetComputeRoot32BitConstant(0, useExactFoot ? 1u : 0u, 0);
			cmd->SetComputeRootDescriptorTable(1, srvHeap->GetGPUDescriptorHandleForHeapStart());
			cmd->SetComputeRootUnorderedAccessView(2, footVectorBuffer->GetGPUVirtualAddress());
			cmd->Dispatch(ThreadGroups, ThreadGroups, ThreadGroups);

			cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(footVectorBuffer.Get(),
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
		}

		DX_API("close upload command list") cmd->Close();
		ID3D12CommandList* lists[] = { cmd.Get() };
		commandQueue->ExecuteCommandLists(1, lists);

		uploadFence.signal(commandQueue, ++uploadFenceValue);
		uploadFence.cpuWait();
	}

	// Reserved for future passes that smooth/filter the footvector field.
	void recordComputeCommands() {
	}

	// Builds this frame's ImGui widgets and appends their draw commands to the
	// still-open graphics command list (backbuffer already bound as render
	// target). Called from PopulateCommandList(), right after the scene draw.
	void BuildImGui() {
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::SetNextWindowSize(ImVec2(220, 0), ImGuiCond_FirstUseEver);
		ImGui::Begin("g-BCC Controls");
		static const char* renderModeItems[] = { "Labels", "Normals", "Distance" };
		ImGui::Combo("Render Mode", &renderMode, renderModeItems, IM_ARRAYSIZE(renderModeItems));
		static const char* initMethodItems[] = { "JFA", "Analytic", "JFA + Filter", "JFA + Quadric" };
		ImGui::Combo("Init Method", &initMethod, initMethodItems, IM_ARRAYSIZE(initMethodItems));
		static const char* testShapeItems[] = { "Torus", "Ellipsoid" };
		ImGui::Combo("Test Shape", &testShapeKind, testShapeItems, IM_ARRAYSIZE(testShapeItems));
		ImGui::TextDisabled("(applied on Reinitialize)");
		if (initMethod == InitMethod_JfaFilter) {
			ImGui::SliderInt("Filter Iterations", &filterIterations, 0, 32);
			ImGui::SliderFloat("Filter Relax Rate", &filterRelaxRate, 0.0f, 1.0f);
		}
		if (initMethod == InitMethod_QuadricOptimization) {
			ImGui::SliderInt("Quadric Iterations", &quadricIterations, 0, 64);
			if (ImGui::CollapsingHeader("Quadric Parameters")) {
				ImGui::SliderFloat("Foot Data Weight", &quadricFootDataWeight, 0.0f, 5.0f);
				ImGui::SliderFloat("Foot Smooth Weight", &quadricFootSmoothWeight, 0.0f, 1.0f);
				ImGui::SliderFloat("Foot Step Damping", &quadricFootStepDamping, 0.0f, 1.0f);
				ImGui::SliderFloat("Min Foot Length", &quadricMinFootLength, 0.0f, 0.1f);
				ImGui::SliderFloat("Tensor Data Weight", &quadricTensorDataWeight, 0.0f, 2.0f);
				ImGui::SliderFloat("Tensor Smooth Weight", &quadricTensorSmoothWeight, 0.0f, 1.0f);
				ImGui::SliderFloat("Tensor Step", &quadricTensorStep, 0.0f, 1.0f);
				ImGui::SliderFloat("Max Tensor Frobenius", &quadricMaxTensorFrobenius, 0.0f, 20.0f);
				ImGui::SliderFloat("Residual Scale", &quadricResidualScale, 0.01f, 5.0f);
				ImGui::SliderFloat("Normal Power", &quadricNormalPower, 0.0f, 16.0f);
				ImGui::SliderFloat("Min Normal Dot", &quadricMinNormalDot, -1.0f, 1.0f);
				ImGui::SliderFloat("Min System Diagonal", &quadricMinSystemDiagonal, 0.0f, 1.0f);
				ImGui::SliderFloat("Max Foot Step", &quadricMaxFootStep, 0.0f, 2.0f);
				ImGui::SliderFloat("Max Residual", &quadricMaxResidual, 0.0f, 10.0f);
				ImGui::Checkbox("Use Residual Normalization", &quadricUseResidualNormalization);
				ImGui::SliderFloat("Cross Label Weight", &quadricCrossLabelWeight, 0.0f, 10.0f);
				ImGui::SliderFloat("Foot Gate Near", &quadricFootGateNear, 0.0f, 10.0f * CellSize);
				ImGui::SliderFloat("Foot Gate Far", &quadricFootGateFar, 0.0f, 10.0f * CellSize);
			}
			if (quadricDataValid) {
				ImGui::SliderInt("Continue Iterations", &quadricContinueIterations, 1, 64);
				if (ImGui::Button("Continue")) needsContinue = true;
				ImGui::SameLine();
				ImGui::TextDisabled("('C' key)");
			}
		}
		if (ImGui::Button("Reinitialize")) needsReinit = true;
		ImGui::SameLine();
		ImGui::TextDisabled("('R' key)");

		ImGui::Separator();
		ImGui::Checkbox("Show Foot Vectors", &showFootLines);
		ImGui::Checkbox("Show Foot Points", &showFootPoints);
		const float worldExtent = GridRes * CellSize;
		ImGui::SliderFloat("Max Vector Length", &footMaxLen, 0.0f, worldExtent * 1.8f);
		ImGui::SliderFloat3("Start Min", &footBoxMin.x, 0.0f, worldExtent);
		ImGui::SliderFloat3("Start Max", &footBoxMax.x, 0.0f, worldExtent);

		// Hidden and (via quadricDataValid) effectively disabled whenever the
		// last init run wasn't QuadricOptimization -- quadricCurrentNodes is
		// the only place the curvature tensor this needs still exists.
		if (quadricDataValid) {
			ImGui::Separator();
			ImGui::Checkbox("Show Foot Quadrics", &showFootQuadrics);
			ImGui::SliderInt("Quadric Patch Stride", &quadricPatchStride, (int)QuadricPatchMinStride, (int)QuadricNodeCount);
			ImGui::SliderInt("Quadric Patch Offset", &quadricPatchOffset, 0, (int)QuadricNodeCount - 1);
			ImGui::SliderFloat("Quadric Patch Radius", &quadricPatchRadius, 0.1f, 10.0f * CellSize);
		}

		ImGui::Text("%.1f FPS", ImGui::GetIO().Framerate);
		ImGui::End();

		ImGui::Render();
		// ImGui needs its own SRV heap bound (for the font texture); the raymarch
		// draw's srvHeap is done being read by this point, so switching is safe.
		ID3D12DescriptorHeap* imguiHeaps[] = { imguiSrvHeap.Get() };
		commandList->SetDescriptorHeaps(1, imguiHeaps);
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.Get());
	}

public:
	BccApp() : SimpleApp() {}

	// Call once after LoadAssets(), from main.cpp where the HWND is available.
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

	// Call before Destroy(), while the device/command queue are still alive.
	void ShutdownImGui() {
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}

	virtual void CreateResources() override {
		Egg::SimpleApp::CreateResources();

		frameCount = 0;
		uploadFence.createResources(device);

		D3D12_DESCRIPTOR_HEAP_DESC uavHeapDesc = {};
		uavHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		uavHeapDesc.NumDescriptors = SlotCount;
		uavHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		DX_API("create BCC uav heap")
			device->CreateDescriptorHeap(&uavHeapDesc, IID_PPV_ARGS(uavHeap.GetAddressOf()));

		D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = uavHeapDesc;
		DX_API("create BCC srv heap")
			device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(srvHeap.GetAddressOf()));

		D3D12_DESCRIPTOR_HEAP_DESC imguiHeapDesc = {};
		imguiHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		imguiHeapDesc.NumDescriptors = 1;
		imguiHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		DX_API("create imgui srv heap")
			device->CreateDescriptorHeap(&imguiHeapDesc, IID_PPV_ARGS(imguiSrvHeap.GetAddressOf()));

		CreateGridTextures();

		{
			D3D12_RESOURCE_DESC bufDesc = CD3DX12_RESOURCE_DESC::Buffer(
				(UINT64)FootVectorCount * FootVectorEntryStride, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
			DX_API("create footvector buffer")
				device->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
					D3D12_HEAP_FLAG_NONE,
					&bufDesc,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
					nullptr,
					IID_PPV_ARGS(footVectorBuffer.ReleaseAndGetAddressOf()));
			footVectorBuffer->SetName(L"footVectorBuffer");
		}

		{
			D3D12_RESOURCE_DESC nodeBufDesc = CD3DX12_RESOURCE_DESC::Buffer(
				(UINT64)QuadricNodeCount * PackedNodeStride, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
			auto createQuadricBuffer = [&](com_ptr<ID3D12Resource>& res, const wchar_t* name) {
				DX_API("create quadric node buffer")
					device->CreateCommittedResource(
						&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
						D3D12_HEAP_FLAG_NONE,
						&nodeBufDesc,
						D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
						nullptr,
						IID_PPV_ARGS(res.ReleaseAndGetAddressOf()));
				res->SetName(name);
			};
			createQuadricBuffer(quadricOriginalNodes, L"quadricOriginalNodes");
			createQuadricBuffer(quadricCurrentNodes, L"quadricCurrentNodes");
			createQuadricBuffer(quadricFootUpdatedNodes, L"quadricFootUpdatedNodes");
			createQuadricBuffer(quadricNormalUpdatedNodes, L"quadricNormalUpdatedNodes");
			createQuadricBuffer(quadricTransportedNodes, L"quadricTransportedNodes");
			createQuadricBuffer(quadricOutputNodes, L"quadricOutputNodes");

			D3D12_RESOURCE_DESC patchSegDesc = CD3DX12_RESOURCE_DESC::Buffer(
				(UINT64)MaxQuadricPatches * QuadricPatchSegmentsPerNode * FootVectorEntryStride,
				D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
			DX_API("create quadric patch segments buffer")
				device->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
					D3D12_HEAP_FLAG_NONE,
					&patchSegDesc,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
					nullptr,
					IID_PPV_ARGS(quadricPatchSegments.ReleaseAndGetAddressOf()));
			quadricPatchSegments->SetName(L"quadricPatchSegments");

			D3D12_RESOURCE_DESC sparseSeedsDesc = CD3DX12_RESOURCE_DESC::Buffer(
				(UINT64)QuadricNodeCount * SparseNodeSeedsStride, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
			DX_API("create sparse node seeds buffer")
				device->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
					D3D12_HEAP_FLAG_NONE,
					&sparseSeedsDesc,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
					nullptr,
					IID_PPV_ARGS(sparseNodeSeeds.ReleaseAndGetAddressOf()));
			sparseNodeSeeds->SetName(L"sparseNodeSeeds");
		}

		torusInitCS.createResources(device, "Shaders/torusInitCS.cso");
		analyticInitCS.createResources(device, "Shaders/analyticInitCS.cso");
		jfaSeedCS.createResources(device, "Shaders/jfaSeedCS.cso");
		jfaStepCS.createResources(device, "Shaders/jfaStepCS.cso");
		jfaFinalizeCS.createResources(device, "Shaders/jfaFinalizeCS.cso");
		labelSnapshotCS.createResources(device, "Shaders/labelSnapshotCS.cso");
		sparseLabelSeedCS.createResources(device, "Shaders/sparseLabelSeedCS.cso");
		sparseLabelClearCS.createResources(device, "Shaders/sparseLabelClearCS.cso");
		sparseLabelHarvestCS.createResources(device, "Shaders/sparseLabelHarvestCS.cso");
		labelPotentialInitCS.createResources(device, "Shaders/labelPotentialInitCS.cso");
		labelPotentialDiffuseCS.createResources(device, "Shaders/labelPotentialDiffuseCS.cso");
		labelPotentialRecoverCS.createResources(device, "Shaders/labelPotentialRecoverCS.cso");
		quadricSeedCS.createResources(device, "Shaders/quadricSeedCS.cso");
		csOffsetGaussNewtonCS.createResources(device, "Shaders/CSOffsetGaussNewton.cso");
		csNormalRotationCS.createResources(device, "Shaders/CSNormalRotation.cso");
		csTransportTensorCS.createResources(device, "Shaders/CSTransportTensor.cso");
		csTensorGradientCS.createResources(device, "Shaders/CSTensorGradient.cso");
		quadricRecoverCS.createResources(device, "Shaders/quadricRecoverCS.cso");
		quadricPatchBuildCS.createResources(device, "Shaders/quadricPatchBuildCS.cso");
		footVectorBuildCS.createResources(device, "Shaders/footVectorBuildCS.cso");

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
			// This pass has no real 3D geometry (full-screen triangle) -- the PS
			// writes the actual per-pixel depth explicitly (see raymarchPS.hlsl's
			// SV_Depth output) from the marched hit position, so the depth test
			// here just needs to always pass and the result always gets written,
			// giving the footvector overlay passes something correct to test against.
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

		// Footvector overlay PSOs: alpha-blended, screen-space quad-expanded
		// (see footLineVS.hlsl/footPointVS.hlsl) so their edges antialias
		// without needing MSAA on the backbuffer.
		{
			D3D12_BLEND_DESC alphaBlend = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			alphaBlend.RenderTarget[0].BlendEnable    = TRUE;
			alphaBlend.RenderTarget[0].SrcBlend       = D3D12_BLEND_SRC_ALPHA;
			alphaBlend.RenderTarget[0].DestBlend      = D3D12_BLEND_INV_SRC_ALPHA;
			alphaBlend.RenderTarget[0].BlendOp        = D3D12_BLEND_OP_ADD;
			alphaBlend.RenderTarget[0].SrcBlendAlpha  = D3D12_BLEND_ONE;
			alphaBlend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
			alphaBlend.RenderTarget[0].BlendOpAlpha   = D3D12_BLEND_OP_ADD;

			auto buildOverlayPso = [&](const char* vsPath, const char* psPath,
				com_ptr<ID3D12RootSignature>& rootSig, com_ptr<ID3D12PipelineState>& pso) {
				com_ptr<ID3DBlob> vs = Egg::Shader::LoadCso(vsPath);
				com_ptr<ID3DBlob> ps = Egg::Shader::LoadCso(psPath);
				rootSig = Egg::Shader::LoadRootSignature(device.Get(), vs.Get());

				D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
				psoDesc.pRootSignature = rootSig.Get();
				psoDesc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
				psoDesc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
				psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
				psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
				// Test against the marched surface's depth (raymarchPS.hlsl writes
				// it explicitly) so the overlay is occluded by nearer surface, but
				// don't write depth themselves -- these are alpha-blended
				// transparent quads, and letting nearby/overlapping ones depth-write
				// against each other would cause arbitrary self-occlusion between
				// footvectors instead of a consistent blend order.
				psoDesc.DepthStencilState.DepthEnable = TRUE;
				psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
				psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
				psoDesc.BlendState = alphaBlend;
				D3D12_INPUT_LAYOUT_DESC emptyLayout = { nullptr, 0 };
				psoDesc.InputLayout = emptyLayout;
				psoDesc.NumRenderTargets = 1;
				psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
				psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
				psoDesc.SampleMask = UINT_MAX;
				psoDesc.SampleDesc.Count = 1;
				DX_API("create footvector overlay PSO")
					device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(pso.GetAddressOf()));
			};

			buildOverlayPso("Shaders/footLineVS.cso", "Shaders/footLinePS.cso", footLineRootSig, footLinePso);
			buildOverlayPso("Shaders/footPointVS.cso", "Shaders/footPointPS.cso", footPointRootSig, footPointPso);
			buildOverlayPso("Shaders/quadricPatchLineVS.cso", "Shaders/quadricPatchLinePS.cso", quadricPatchLineRootSig, quadricPatchLinePso);
		}

		D3D12_COMMAND_QUEUE_DESC descCommandQueue = { D3D12_COMMAND_LIST_TYPE_COMPUTE, 0, D3D12_COMMAND_QUEUE_FLAG_NONE };
		DX_API("create compute command queue.")
			device->CreateCommandQueue(&descCommandQueue, IID_PPV_ARGS(computeCommandQueue.ReleaseAndGetAddressOf()));

		DX_API("create upload command allocator.")
			device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
				IID_PPV_ARGS(uploadAllocator.ReleaseAndGetAddressOf()));

		DX_API("create upload command list.")
			device->CreateCommandList(
				0,
				D3D12_COMMAND_LIST_TYPE_DIRECT,
				uploadAllocator.Get(),
				nullptr,
				IID_PPV_ARGS(uploadCommandList.ReleaseAndGetAddressOf()));

		DX_API("close command list.")
			uploadCommandList->Close();
		ID3D12CommandList* ppCommandLists[] = { uploadCommandList.Get() };
		commandQueue->ExecuteCommandLists(1, ppCommandLists);

		uploadFence.signal(commandQueue, ++uploadFenceValue);
		uploadFence.cpuWait();
	}

	virtual void CreateSwapChainResources() override {
		__super::CreateSwapChainResources();

		computeFenceChain.createResources(device, swapChainBackBufferCount);
		graphicsFenceChain.createResources(device, swapChainBackBufferCount);

		computeCommandLists.resize(swapChainBackBufferCount);
		computeAllocators.resize(swapChainBackBufferCount);
		for (uint i = 0; i < swapChainBackBufferCount; i++) {
			DX_API("create compute command allocator.")
				device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE,
					IID_PPV_ARGS(computeAllocators[i].ReleaseAndGetAddressOf()));

			DX_API("create compute command list.")
				device->CreateCommandList(
					0,
					D3D12_COMMAND_LIST_TYPE_COMPUTE,
					computeAllocators[i].Get(),
					nullptr,
					IID_PPV_ARGS(computeCommandLists[i].ReleaseAndGetAddressOf()));
			computeCommandLists[i]->Close();
		}

		if (camera) camera->SetAspect(aspectRatio);
	}

	virtual void LoadAssets() override {
		using namespace Egg::Math;

		frameCb.CreateResources(device.Get());
		torusCb.CreateResources(device.Get());
		footVizCb.CreateResources(device.Get());
		quadricCb.CreateResources(device.Get());

		BuildShapeList();
		torusCb.Upload();

		camera = Egg::Cam::FirstPerson::Create();
		camera->SetProj(0.9f, aspectRatio, 0.1f, MaxMarchDist);
		camera->SetAspect(aspectRatio);
		camera->SetSpeed(0.15f * GridRes);
		float3 center(GridRes * CellSize * 0.5f, GridRes * CellSize * 0.5f, GridRes * CellSize * 0.5f);
		float3 eye = center + float3(0.0f, GridRes * 1.1f, GridRes * -1.8f);
		camera->SetView(eye, (center - eye).Normalize());

		RunInitPasses();
	}

	virtual void Render() override {
		if (needsReinit) {
			RunInitPasses();
			needsReinit = false;
		} else if (needsContinue) {
			RunInitPasses(true);
			needsContinue = false;
		}

		// --- Compute pass (currently a no-op stub, see recordComputeCommands) ---
		computeAllocators[swapChainBackBufferIndex]->Reset();
		auto& computeCmd = computeCommandLists[swapChainBackBufferIndex];
		computeCmd->Reset(computeAllocators[swapChainBackBufferIndex].Get(), nullptr);

		recordComputeCommands();

		DX_API("close compute command list") computeCmd->Close();

		graphicsFenceChain.gpuWait(computeCommandQueue, swapChainBackBufferIndex);
		{
			ID3D12CommandList* lists[] = { computeCmd.Get() };
			computeCommandQueue->ExecuteCommandLists(1, lists);
		}
		computeFenceChain.signal(computeCommandQueue, swapChainBackBufferIndex);

		computeFenceChain.gpuWait(commandQueue, swapChainBackBufferIndex);

		// --- Graphics pass: sphere-traced visualization ---
		PopulateCommandList();

		ID3D12CommandList* cLists[] = { commandList.Get() };
		commandQueue->ExecuteCommandLists(1, cLists);

		graphicsFenceChain.signal(commandQueue, swapChainBackBufferIndex);

		DX_API("Failed to present swap chain")
			swapChain->Present(1, 0);

		WaitForPreviousFrame();

		frameCount++;
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

		ID3D12DescriptorHeap* heaps[] = { srvHeap.Get() };
		commandList->SetDescriptorHeaps(1, heaps);
		commandList->SetGraphicsRootSignature(raymarchRootSig.Get());
		commandList->SetGraphicsRootConstantBufferView(0, frameCb.GetGPUVirtualAddress());
		commandList->SetGraphicsRootDescriptorTable(1, srvHeap->GetGPUDescriptorHandleForHeapStart());
		commandList->SetPipelineState(raymarchPso.Get());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->DrawInstanced(3, 1, 0, 0);

		// -- footvector visualization overlays: quad-expanded, alpha-blended,
		// no depth test (this app has no depth buffer), so they always draw on
		// top of the raymarched surface. Filtered out entries collapse to a
		// degenerate primitive in the VS, so it's always safe to draw the full
		// FootVectorCount instances regardless of how many pass the filter. --
		if (showFootLines) {
			commandList->SetGraphicsRootSignature(footLineRootSig.Get());
			commandList->SetGraphicsRootConstantBufferView(0, frameCb.GetGPUVirtualAddress());
			commandList->SetGraphicsRootConstantBufferView(1, footVizCb.GetGPUVirtualAddress());
			commandList->SetGraphicsRootShaderResourceView(2, footVectorBuffer->GetGPUVirtualAddress());
			commandList->SetPipelineState(footLinePso.Get());
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			commandList->DrawInstanced(4, FootVectorCount, 0, 0);
		}
		if (showFootPoints) {
			commandList->SetGraphicsRootSignature(footPointRootSig.Get());
			commandList->SetGraphicsRootConstantBufferView(0, frameCb.GetGPUVirtualAddress());
			commandList->SetGraphicsRootConstantBufferView(1, footVizCb.GetGPUVirtualAddress());
			commandList->SetGraphicsRootShaderResourceView(2, footVectorBuffer->GetGPUVirtualAddress());
			commandList->SetPipelineState(footPointPso.Get());
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			commandList->DrawInstanced(4, FootVectorCount, 0, 0);
		}

		// -- foot-quadric wireframe overlay: only meaningful right after a
		// QuadricOptimization run (quadricDataValid), and only when the user
		// has turned it on. Rebuilds the (small, fixed-capacity) segment
		// buffer fresh every frame directly on this graphics command list --
		// cheap enough not to need the async compute queue -- then draws it
		// with the same antialiased screen-space-quad line technique as the
		// footvector overlay. See quadricPatchBuildCS.hlsl. --
		if (quadricDataValid && showFootQuadrics) {
			uint stride = (quadricPatchStride > (int)QuadricPatchMinStride) ? (uint)quadricPatchStride : QuadricPatchMinStride;
			uint offset = (quadricPatchOffset > 0) ? (uint)quadricPatchOffset : 0u;
			uint numSelected = 0;
			if (offset < QuadricNodeCount) {
				numSelected = (QuadricNodeCount - offset + stride - 1) / stride;
				if (numSelected > MaxQuadricPatches) numSelected = MaxQuadricPatches;
			}

			if (numSelected > 0) {
				float radius = quadricPatchRadius;
				uint radiusBits = *reinterpret_cast<uint*>(&radius);
				uint dispatchGroups = (numSelected + 63) / 64;

				commandList->SetComputeRootSignature(quadricPatchBuildCS.rootSig.Get());
				commandList->SetPipelineState(quadricPatchBuildCS.pso.Get());
				commandList->SetComputeRootConstantBufferView(0, quadricCb.GetGPUVirtualAddress());
				commandList->SetComputeRoot32BitConstant(1, offset, 0);
				commandList->SetComputeRoot32BitConstant(1, stride, 1);
				commandList->SetComputeRoot32BitConstant(1, numSelected, 2);
				commandList->SetComputeRoot32BitConstant(1, radiusBits, 3);
				commandList->SetComputeRootShaderResourceView(2, quadricCurrentNodes->GetGPUVirtualAddress());
				commandList->SetComputeRootShaderResourceView(3, sparseNodeSeeds->GetGPUVirtualAddress());
				commandList->SetComputeRootUnorderedAccessView(4, quadricPatchSegments->GetGPUVirtualAddress());
				commandList->Dispatch(dispatchGroups, 1, 1);

				commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(quadricPatchSegments.Get(),
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

				commandList->SetGraphicsRootSignature(quadricPatchLineRootSig.Get());
				commandList->SetGraphicsRootConstantBufferView(0, frameCb.GetGPUVirtualAddress());
				commandList->SetGraphicsRootConstantBufferView(1, footVizCb.GetGPUVirtualAddress());
				commandList->SetGraphicsRootShaderResourceView(2, quadricPatchSegments->GetGPUVirtualAddress());
				commandList->SetPipelineState(quadricPatchLinePso.Get());
				commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				commandList->DrawInstanced(4, numSelected * QuadricPatchSegmentsPerNode, 0, 0);

				commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(quadricPatchSegments.Get(),
					D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
			}
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
		frameCb.data.ahead             = float4(camera->GetAhead(), 0.0f);
		frameCb.data.gridParams        = float4((float)GridRes, CellSize, (float)MaxMarchSteps, MaxMarchDist);
		frameCb.data.renderParams      = float4((float)renderMode, 0.0f, 0.0f, 0.0f);
		frameCb.Upload();

		footVizCb.data.boxMin   = float4(footBoxMin, footMaxLen);
		footVizCb.data.boxMax   = float4(footBoxMax, FootLineHalfWidthPx);
		footVizCb.data.viewport = float4(viewPort.Width, viewPort.Height, FootPointRadiusPx, 0.0f);
		footVizCb.Upload();
	}

	virtual void ProcessMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override {
		if (camera) camera->ProcessMessage(hWnd, uMsg, wParam, lParam);

		// 'R' = Reinitialize, 'C' = Continue (see the matching ImGui buttons in
		// BuildImGui()), '1'-'9' = set both the Quadric Iterations and
		// Continue Iterations sliders to that digit -- ignored while ImGui
		// wants keyboard focus (e.g. a slider's Ctrl+Click numeric entry) so
		// shortcuts don't fight typing.
		if (uMsg == WM_KEYDOWN && ImGui::GetCurrentContext() && !ImGui::GetIO().WantCaptureKeyboard) {
			if (wParam == 'R') {
				needsReinit = true;
			} else if (wParam == 'C' && quadricDataValid && initMethod == InitMethod_QuadricOptimization) {
				needsContinue = true;
			} else if (wParam >= '1' && wParam <= '9') {
				int digit = (int)(wParam - '0');
				quadricIterations = digit;
				quadricContinueIterations = digit;
			}
		}
	}

	virtual void ReleaseSwapChainResources() override {
		Egg::SimpleApp::ReleaseSwapChainResources();
	}

	virtual void ReleaseResources() override {
		frameCb.ReleaseResources();
		torusCb.ReleaseResources();
		footVizCb.ReleaseResources();
		quadricCb.ReleaseResources();
		for (uint i = 0; i < SlotCount; i++) gridTex[i].Reset();
		footVectorBuffer.Reset();
		quadricOriginalNodes.Reset();
		quadricCurrentNodes.Reset();
		quadricFootUpdatedNodes.Reset();
		quadricNormalUpdatedNodes.Reset();
		quadricTransportedNodes.Reset();
		quadricOutputNodes.Reset();
		sparseNodeSeeds.Reset();
		quadricPatchSegments.Reset();
		quadricPatchLineRootSig.Reset();
		quadricPatchLinePso.Reset();
		uavHeap.Reset();
		srvHeap.Reset();
		imguiSrvHeap.Reset();
		raymarchRootSig.Reset();
		raymarchPso.Reset();
		footLineRootSig.Reset();
		footLinePso.Reset();
		footPointRootSig.Reset();
		footPointPso.Reset();
		Egg::SimpleApp::ReleaseResources();
	}
};
