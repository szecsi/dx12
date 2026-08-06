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

#include "Shaders/AequorConfig.hlsli"
#include "Shaders/AequorCb.hlsli"
#include "Shaders/AequorFrameCb.hlsli"
#include "Shaders/TorusListCb.hlsli"

#include "Common/DescriptorAllocator.h"
#include "Common/GpuBuffer.h"
#include "Common/DoubleBufferGpuBuffer.h"
#include "Common/SpatialGrid.h"
#include "AequorParticleTypes.h"

#include <vector>

// Must match TorusListCb.ShapeKind's 0/1 convention (same enum g-BCC uses).
enum AequorTestShape { TestShape_Torus = 0, TestShape_Ellipsoid = 1 };

// g-Aequor: a Position-Based-Fluids-style particle relaxation replacing
// g-BCC's lattice-based multi-label footvector field. Particles (position,
// normal, curvature tensor, label) are spawned at boundary voxels of the
// same synthetic torus/ellipsoid test scene, then relaxed every iteration by
// 4 constraints: any-label surface positioning, same-label quadric
// consistency, same-label spacing (PBF density), and a same-label one-sided
// "stay within AnchorRadius of some ground-truth voxel of my own label"
// barrier. See the g-Aequor design plan for the full rationale.
class AequorApp : public Egg::SimpleApp {

	static constexpr uint GridRes = 64;          // must match AequorConfig.hlsli's GRID_DIM
	static constexpr float CellSize = 1.0f;      // must match AequorConfig.hlsli's H/CELL_PER_H
	static constexpr uint MaxParticles = 16384;  // must match AequorConfig.hlsli's MAX_PARTICLES
	static constexpr uint SparseLabelCount = 4;  // K labels swept by the occurrence field

	static constexpr uint ThreadGroups = (GridRes + 3) / 4;              // numthreads(4,4,4) dispatches
	static constexpr uint ParticleGroups = (MaxParticles + THREAD_GROUP_SIZE - 1) / THREAD_GROUP_SIZE;

	static constexpr float MaxMarchDist = GridRes * CellSize * 3.0f;
	static constexpr int MaxMarchSteps = 256;
	static constexpr float PointRadiusPx = 3.0f;

	// -- quadric-patch wireframe overlay (quadricPatchBuildCS.hlsl): for a
	// stride-selected subset of particles, draws the actual local quadric
	// surface each one's (normal, curvature tensor) defines, clipped to a
	// radius -- ported from g-BCC's equivalent overlay. --
	static constexpr uint QuadricPatchRingSamples = 16; // must match quadricPatchBuildCS.hlsl's RingSamples
	static constexpr uint QuadricPatchSegmentsPerNode = 2 * (QuadricPatchRingSamples - 1) + QuadricPatchRingSamples;
	static constexpr uint MaxQuadricPatches = 2048; // fixed buffer capacity
	static constexpr uint QuadricPatchMinStride = (MaxParticles + MaxQuadricPatches - 1) / MaxQuadricPatches;

	uint frameCount = 0;

	Egg::Cam::FirstPerson::P camera;

protected:
	com_ptr<ID3D12CommandAllocator>    uploadAllocator;
	com_ptr<ID3D12GraphicsCommandList> uploadCommandList;
	Egg::Compute::Fence uploadFence;
	uint64_t uploadFenceValue = 0;

	// -- spatial proximity search + particle storage, reused from C:\Coding\pbf\pbf --
	DescriptorAllocator::P mainAlloc;   // shader-visible, SpatialGrid's own internal dispatches
	DescriptorAllocator::P staticAlloc; // CPU-only staging heap for the same
	SpatialGrid::P spatialGrid;
	DoubleBufferGpuBuffer::P particleFields[PF_COUNT]; // Position, Normal, TensorDiag, TensorOffdiag, Label

	// Plain (non-double-buffered) scratch -- always freshly written within a
	// single pass, never needs to survive a sort. Also RasterLabel/Occ0/Occ1/
	// OccurrenceSeed/Counter: all bound as UAV everywhere they're touched
	// (compute AND, for Position/Label, the point-rendering VS too), so none
	// of these ever need a resource-state transition after creation.
	com_ptr<ID3D12Resource> scratchVec3;
	com_ptr<ID3D12Resource> scratchTensorDiag;
	com_ptr<ID3D12Resource> scratchTensorOffdiag;
	com_ptr<ID3D12Resource> scratchLambda;
	com_ptr<ID3D12Resource> counterBuffer;      // 1 uint: spawnCS's next-free-slot atomic counter
	com_ptr<ID3D12Resource> rasterLabelBuffer;  // GridRes^3 uint: ground-truth per-voxel label
	com_ptr<ID3D12Resource> occ0Buffer;         // GridRes^3 uint2: occurrence-JFA ping
	com_ptr<ID3D12Resource> occ1Buffer;         // GridRes^3 uint2: occurrence-JFA pong
	com_ptr<ID3D12Resource> occurrenceSeedBuffer; // GridRes^3 * SparseLabelCount uint2, persistent

	Egg::Compute::ComputeShader rasterLabelCS;
	Egg::Compute::ComputeShader occSeedCS;
	Egg::Compute::ComputeShader occStepCS;
	Egg::Compute::ComputeShader occFinalizeCS;
	Egg::Compute::ComputeShader clearParticlesCS;
	Egg::Compute::ComputeShader spawnCS;
	Egg::Compute::ComputeShader surfaceConstraintCS;
	Egg::Compute::ComputeShader quadricConsistencyCS;
	Egg::Compute::ComputeShader spacingLambdaCS;
	Egg::Compute::ComputeShader spacingDeltaCS;
	Egg::Compute::ComputeShader anchorBarrierCS;
	Egg::Compute::ComputeShader commitPositionCS;
	Egg::Compute::ComputeShader commitTensorCS;
	Egg::Compute::ComputeShader copyParticlesCS;
	Egg::Compute::ComputeShader quadricPatchBuildCS;

	com_ptr<ID3D12RootSignature> raymarchRootSig;
	com_ptr<ID3D12PipelineState> raymarchPso;
	com_ptr<ID3D12RootSignature> particlePointRootSig;
	com_ptr<ID3D12PipelineState> particlePointPso;
	com_ptr<ID3D12RootSignature> quadricPatchLineRootSig;
	com_ptr<ID3D12PipelineState> quadricPatchLinePso;
	com_ptr<ID3D12Resource> quadricPatchSegments;

	bool showFootQuadrics = false;
	int quadricPatchStride = 64;
	int quadricPatchOffset = 0;
	float quadricPatchRadius = 2.0f * CellSize;

	Egg::ConstantBuffer<AequorFrameCb> frameCb;
	Egg::ConstantBuffer<AequorCb>      aequorCb;
	Egg::ConstantBuffer<TorusListCb>   torusCb;

	com_ptr<ID3D12DescriptorHeap> imguiSrvHeap;

	int testShapeKind = TestShape_Torus; // driven by the ImGui combo, applied on Reinitialize only (BuildShapeList)
	bool needsReinit = false;
	bool needsContinue = false;
	bool dataValid = false; // true once a Reinit/Continue has actually produced particles

	int iterations = 8;
	int continueIterations = 8;

	float anchorRadius = 1.5f;
	float surfaceDataWeight = 1.0f;
	float surfaceStepDamping = 0.5f;
	// Regularization floor for surfaceConstraintCS's Gauss-Newton denominator
	// -- was 0.001, far too permissive: a particle with a thin/near-empty
	// weighted neighborhood (small sumWGG) could take an arbitrarily large
	// step. 0.25 keeps single-iteration corrections bounded to a fraction of
	// a cell even in that case.
	float minSystemDiagonal = 0.25f;
	float quadricConsistencyWeight = 0.3f;
	float maxTensorFrobenius = 4.0f;
	float spacingEpsilon = 100.0f;
	float residualScale = 0.5f;
	float normalPower = 0.0f;
	float minNormalDot = -1.0f;
	float maxResidual = 2.0f;
	bool useResidualNormalization = false;
	float footGateNear = 2.0f * CellSize;
	float footGateFar = 3.0f * CellSize;
	// Hard per-iteration displacement cap for every position-correcting
	// constraint -- see AequorCb.hlsli's MaxStep doc. Without this, more
	// iterations made things WORSE instead of converging (an unclamped
	// overshoot compounds every iteration); same safety role as g-BCC's
	// MaxFootStep.
	float maxStep = 0.25f * CellSize;

	// Test shape: rebuilds torusCb.data entirely from testShapeKind -- same
	// scene as g-BCC's BuildShapeList, but centered at the origin (no
	// lattice here, so no reason to avoid negative coordinates).
	void BuildShapeList() {
		using namespace Egg::Math;
		float3 center(0.0f, 0.0f, 0.0f);

		torusCb.data.ShapeKind = (uint)testShapeKind;

		if (testShapeKind == TestShape_Ellipsoid) {
			torusCb.data.nTorii = 1;
			torusCb.data.torii[0].center      = center;
			torusCb.data.torii[0].axis        = float3(10.0f, 7.0f, 5.0f);
			torusCb.data.torii[0].majorRadius = 0.0f;
			torusCb.data.torii[0].minorRadius = 0.0f;
			torusCb.data.torii[0].label       = 1;
			return;
		}

		torusCb.data.nTorii = 1;
		torusCb.data.torii[0].center      = center;
		torusCb.data.torii[0].axis        = float3(0, 1, 0);
		torusCb.data.torii[0].majorRadius = 14.0f;
		torusCb.data.torii[0].minorRadius = 5.0f;
		torusCb.data.torii[0].label       = 1;
	}

	void UploadAequorCb() {
		aequorCb.data.numParticles = MaxParticles;
		aequorCb.data.activeParticleCount = MaxParticles; // not read back (v1 scope), purely informational
		aequorCb.data.AnchorRadius = anchorRadius;
		aequorCb.data.MinSystemDiagonal = minSystemDiagonal;
		aequorCb.data.SurfaceDataWeight = surfaceDataWeight;
		aequorCb.data.SurfaceStepDamping = surfaceStepDamping;
		aequorCb.data.QuadricConsistencyWeight = quadricConsistencyWeight;
		aequorCb.data.MaxTensorFrobenius = maxTensorFrobenius;
		aequorCb.data.SpacingEpsilon = spacingEpsilon;
		aequorCb.data.ResidualScale = residualScale;
		aequorCb.data.NormalPower = normalPower;
		aequorCb.data.MinNormalDot = minNormalDot;
		aequorCb.data.MaxResidual = maxResidual;
		aequorCb.data.UseResidualNormalization = useResidualNormalization ? 1u : 0u;
		aequorCb.data.FootGateNear = footGateNear;
		aequorCb.data.FootGateFar = footGateFar;
		aequorCb.data.MaxStep = maxStep;
		aequorCb.Upload();
	}

	static com_ptr<ID3D12Resource> CreateRawUavBuffer(ID3D12Device* device, UINT64 sizeBytes, const wchar_t* name) {
		com_ptr<ID3D12Resource> res;
		D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(sizeBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		DX_API("create g-Aequor raw UAV buffer")
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

	// -- occurrence-distance field: for each label, "nearest voxel of my own
	// label" -- occurrence-seeded (every voxel of that label, not just its
	// boundary), single simple-cubic grid, reusing the JFA propagation idea
	// from g-BCC (occStepCS.hlsl's step schedule matches jfaStepCS.hlsl's). --
	void RunOccurrenceFieldInit(com_ptr<ID3D12GraphicsCommandList>& cmd) {
		cmd->SetComputeRootSignature(rasterLabelCS.rootSig.Get());
		cmd->SetPipelineState(rasterLabelCS.pso.Get());
		cmd->SetComputeRootConstantBufferView(0, aequorCb.GetGPUVirtualAddress());
		cmd->SetComputeRootConstantBufferView(1, torusCb.GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(2, rasterLabelBuffer->GetGPUVirtualAddress());
		cmd->Dispatch(ThreadGroups, ThreadGroups, ThreadGroups);
		cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(rasterLabelBuffer.Get()));

		std::vector<uint> stepSizes;
		for (uint s = GridRes / 2; s >= 1; s /= 2) stepSizes.push_back(s);
		stepSizes.push_back(1);
		stepSizes.push_back(1);

		for (uint label = 0; label < SparseLabelCount; label++) {
			cmd->SetComputeRootSignature(occSeedCS.rootSig.Get());
			cmd->SetPipelineState(occSeedCS.pso.Get());
			cmd->SetComputeRoot32BitConstant(0, label, 0);
			cmd->SetComputeRootUnorderedAccessView(1, rasterLabelBuffer->GetGPUVirtualAddress());
			cmd->SetComputeRootUnorderedAccessView(2, occ0Buffer->GetGPUVirtualAddress());
			cmd->Dispatch(ThreadGroups, ThreadGroups, ThreadGroups);
			cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(occ0Buffer.Get()));

			uint pingIndex = 0;
			cmd->SetComputeRootSignature(occStepCS.rootSig.Get());
			cmd->SetPipelineState(occStepCS.pso.Get());
			for (uint s : stepSizes) {
				uint consts[2] = { s, pingIndex };
				cmd->SetComputeRoot32BitConstants(0, 2, consts, 0);
				cmd->SetComputeRootUnorderedAccessView(1, occ0Buffer->GetGPUVirtualAddress());
				cmd->SetComputeRootUnorderedAccessView(2, occ1Buffer->GetGPUVirtualAddress());
				cmd->Dispatch(ThreadGroups, ThreadGroups, ThreadGroups);
				D3D12_RESOURCE_BARRIER b[] = {
					CD3DX12_RESOURCE_BARRIER::UAV(occ0Buffer.Get()),
					CD3DX12_RESOURCE_BARRIER::UAV(occ1Buffer.Get()),
				};
				cmd->ResourceBarrier(_countof(b), b);
				pingIndex = 1 - pingIndex;
			}

			cmd->SetComputeRootSignature(occFinalizeCS.rootSig.Get());
			cmd->SetPipelineState(occFinalizeCS.pso.Get());
			uint consts[2] = { pingIndex, label };
			cmd->SetComputeRoot32BitConstants(0, 2, consts, 0);
			cmd->SetComputeRootUnorderedAccessView(1, occ0Buffer->GetGPUVirtualAddress());
			cmd->SetComputeRootUnorderedAccessView(2, occ1Buffer->GetGPUVirtualAddress());
			cmd->SetComputeRootUnorderedAccessView(3, occurrenceSeedBuffer->GetGPUVirtualAddress());
			cmd->Dispatch(ThreadGroups, ThreadGroups, ThreadGroups);
			cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(occurrenceSeedBuffer.Get()));
		}
	}

	void RunSpawn(com_ptr<ID3D12GraphicsCommandList>& cmd) {
		cmd->SetComputeRootSignature(clearParticlesCS.rootSig.Get());
		cmd->SetPipelineState(clearParticlesCS.pso.Get());
		cmd->SetComputeRootConstantBufferView(0, aequorCb.GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(1, particleFields[PF_POSITION]->getFront()->Get()->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(2, particleFields[PF_NORMAL]->getFront()->Get()->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(3, particleFields[PF_TENSOR_DIAG]->getFront()->Get()->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(4, particleFields[PF_TENSOR_OFFDIAG]->getFront()->Get()->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(5, particleFields[PF_LABEL]->getFront()->Get()->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(6, counterBuffer->GetGPUVirtualAddress());
		cmd->Dispatch(ParticleGroups, 1, 1);

		D3D12_RESOURCE_BARRIER clearBarriers[] = {
			CD3DX12_RESOURCE_BARRIER::UAV(particleFields[PF_POSITION]->getFront()->Get()),
			CD3DX12_RESOURCE_BARRIER::UAV(particleFields[PF_NORMAL]->getFront()->Get()),
			CD3DX12_RESOURCE_BARRIER::UAV(particleFields[PF_TENSOR_DIAG]->getFront()->Get()),
			CD3DX12_RESOURCE_BARRIER::UAV(particleFields[PF_TENSOR_OFFDIAG]->getFront()->Get()),
			CD3DX12_RESOURCE_BARRIER::UAV(particleFields[PF_LABEL]->getFront()->Get()),
			CD3DX12_RESOURCE_BARRIER::UAV(counterBuffer.Get()),
		};
		cmd->ResourceBarrier(_countof(clearBarriers), clearBarriers);

		cmd->SetComputeRootSignature(spawnCS.rootSig.Get());
		cmd->SetPipelineState(spawnCS.pso.Get());
		cmd->SetComputeRootConstantBufferView(0, aequorCb.GetGPUVirtualAddress());
		cmd->SetComputeRootConstantBufferView(1, torusCb.GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(2, rasterLabelBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(3, counterBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(4, particleFields[PF_POSITION]->getFront()->Get()->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(5, particleFields[PF_NORMAL]->getFront()->Get()->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(6, particleFields[PF_TENSOR_DIAG]->getFront()->Get()->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(7, particleFields[PF_TENSOR_OFFDIAG]->getFront()->Get()->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(8, particleFields[PF_LABEL]->getFront()->Get()->GetGPUVirtualAddress());
		cmd->Dispatch(ThreadGroups, ThreadGroups, ThreadGroups);

		D3D12_RESOURCE_BARRIER spawnBarriers[] = {
			CD3DX12_RESOURCE_BARRIER::UAV(particleFields[PF_POSITION]->getFront()->Get()),
			CD3DX12_RESOURCE_BARRIER::UAV(particleFields[PF_NORMAL]->getFront()->Get()),
			CD3DX12_RESOURCE_BARRIER::UAV(particleFields[PF_TENSOR_DIAG]->getFront()->Get()),
			CD3DX12_RESOURCE_BARRIER::UAV(particleFields[PF_TENSOR_OFFDIAG]->getFront()->Get()),
			CD3DX12_RESOURCE_BARRIER::UAV(particleFields[PF_LABEL]->getFront()->Get()),
		};
		cmd->ResourceBarrier(_countof(spawnBarriers), spawnBarriers);
	}

	// One outer relaxation iteration: rebuild the spatial grid (particles
	// moved since last iteration), then the 4 constraints in sequence, each
	// as its own compute-then-Jacobi-safe-commit pair -- see the g-Aequor
	// plan's "per-iteration constraint pipeline" section.
	void RunOneIteration(com_ptr<ID3D12GraphicsCommandList>& cmd) {
		spatialGrid->Build(cmd.Get());

		// SpatialGrid's internal per-shader UAV barriers (dispatch_then_barrier)
		// target resource pointers captured ONCE at SpatialGrid::Create time,
		// when particleFields[f]->getFront()/getBack() pointed at buffers[0]/[1]
		// respectively. Cover both physical buffers of every field explicitly
		// here rather than trusting that bookkeeping to still be accurate.
		{
			D3D12_RESOURCE_BARRIER b[2 * PF_COUNT];
			uint n = 0;
			for (uint f = 0; f < PF_COUNT; f++) {
				b[n++] = CD3DX12_RESOURCE_BARRIER::UAV(particleFields[f]->getFront()->Get());
				b[n++] = CD3DX12_RESOURCE_BARRIER::UAV(particleFields[f]->getBack()->Get());
			}
			cmd->ResourceBarrier(n, b);
		}

		// Copy the sorted result (SpatialGrid's "back") into a fixed set of
		// physical buffers -- particleFields[f]->getFront() -- via a compute
		// pass, instead of DoubleBufferGpuBuffer::flip()'s pointer-swap.
		// "front" was observed to sometimes render stale (pre-iteration) data
		// depending on how many total iterations had run, i.e. on frontIdx's
		// parity; copying explicitly means every single piece of code outside
		// SpatialGrid's own internal dispatches -- every constraint pass and
		// the render pass -- always reads/writes the SAME physical resource
		// for "the particle state", with nothing depending on how many flips
		// have happened before. frontIdx itself is simply never toggled again.
		cmd->SetComputeRootSignature(copyParticlesCS.rootSig.Get());
		cmd->SetPipelineState(copyParticlesCS.pso.Get());
		cmd->SetComputeRootConstantBufferView(0, aequorCb.GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(1, particleFields[PF_POSITION]->getBack()->Get()->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(2, particleFields[PF_NORMAL]->getBack()->Get()->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(3, particleFields[PF_TENSOR_DIAG]->getBack()->Get()->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(4, particleFields[PF_TENSOR_OFFDIAG]->getBack()->Get()->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(5, particleFields[PF_LABEL]->getBack()->Get()->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(6, particleFields[PF_POSITION]->getFront()->Get()->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(7, particleFields[PF_NORMAL]->getFront()->Get()->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(8, particleFields[PF_TENSOR_DIAG]->getFront()->Get()->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(9, particleFields[PF_TENSOR_OFFDIAG]->getFront()->Get()->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(10, particleFields[PF_LABEL]->getFront()->Get()->GetGPUVirtualAddress());
		cmd->Dispatch(ParticleGroups, 1, 1);
		{
			D3D12_RESOURCE_BARRIER b[PF_COUNT];
			for (uint f = 0; f < PF_COUNT; f++)
				b[f] = CD3DX12_RESOURCE_BARRIER::UAV(particleFields[f]->getFront()->Get());
			cmd->ResourceBarrier(PF_COUNT, b);
		}

		D3D12_GPU_VIRTUAL_ADDRESS posVa   = particleFields[PF_POSITION]->getFront()->Get()->GetGPUVirtualAddress();
		D3D12_GPU_VIRTUAL_ADDRESS normVa  = particleFields[PF_NORMAL]->getFront()->Get()->GetGPUVirtualAddress();
		D3D12_GPU_VIRTUAL_ADDRESS diagVa  = particleFields[PF_TENSOR_DIAG]->getFront()->Get()->GetGPUVirtualAddress();
		D3D12_GPU_VIRTUAL_ADDRESS offVa   = particleFields[PF_TENSOR_OFFDIAG]->getFront()->Get()->GetGPUVirtualAddress();
		D3D12_GPU_VIRTUAL_ADDRESS labelVa = particleFields[PF_LABEL]->getFront()->Get()->GetGPUVirtualAddress();
		D3D12_GPU_VIRTUAL_ADDRESS cellCountVa = spatialGrid->GetCellCountBuffer()->Get()->GetGPUVirtualAddress();
		D3D12_GPU_VIRTUAL_ADDRESS cellPrefixVa = spatialGrid->GetPrefixSumBuffer()->Get()->GetGPUVirtualAddress();
		D3D12_GPU_VIRTUAL_ADDRESS cbVa = aequorCb.GetGPUVirtualAddress();

		auto uavAll = [&]() {
			D3D12_RESOURCE_BARRIER b[] = {
				CD3DX12_RESOURCE_BARRIER::UAV(particleFields[PF_POSITION]->getFront()->Get()),
				CD3DX12_RESOURCE_BARRIER::UAV(particleFields[PF_NORMAL]->getFront()->Get()),
				CD3DX12_RESOURCE_BARRIER::UAV(particleFields[PF_TENSOR_DIAG]->getFront()->Get()),
				CD3DX12_RESOURCE_BARRIER::UAV(particleFields[PF_TENSOR_OFFDIAG]->getFront()->Get()),
			};
			cmd->ResourceBarrier(_countof(b), b);
		};

		// -- surface positioning (any label) --
		cmd->SetComputeRootSignature(surfaceConstraintCS.rootSig.Get());
		cmd->SetPipelineState(surfaceConstraintCS.pso.Get());
		cmd->SetComputeRootConstantBufferView(0, cbVa);
		cmd->SetComputeRootUnorderedAccessView(1, posVa);
		cmd->SetComputeRootUnorderedAccessView(2, normVa);
		cmd->SetComputeRootUnorderedAccessView(3, diagVa);
		cmd->SetComputeRootUnorderedAccessView(4, offVa);
		cmd->SetComputeRootUnorderedAccessView(5, labelVa);
		cmd->SetComputeRootUnorderedAccessView(6, cellCountVa);
		cmd->SetComputeRootUnorderedAccessView(7, cellPrefixVa);
		cmd->SetComputeRootUnorderedAccessView(8, scratchVec3->GetGPUVirtualAddress());
		cmd->Dispatch(ParticleGroups, 1, 1);
		cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(scratchVec3.Get()));

		cmd->SetComputeRootSignature(commitPositionCS.rootSig.Get());
		cmd->SetPipelineState(commitPositionCS.pso.Get());
		cmd->SetComputeRootConstantBufferView(0, cbVa);
		cmd->SetComputeRootUnorderedAccessView(1, scratchVec3->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(2, posVa);
		cmd->Dispatch(ParticleGroups, 1, 1);
		uavAll();

		// -- quadric consistency (same label): normal + curvature tensor --
		cmd->SetComputeRootSignature(quadricConsistencyCS.rootSig.Get());
		cmd->SetPipelineState(quadricConsistencyCS.pso.Get());
		cmd->SetComputeRootConstantBufferView(0, cbVa);
		cmd->SetComputeRootUnorderedAccessView(1, posVa);
		cmd->SetComputeRootUnorderedAccessView(2, normVa);
		cmd->SetComputeRootUnorderedAccessView(3, diagVa);
		cmd->SetComputeRootUnorderedAccessView(4, offVa);
		cmd->SetComputeRootUnorderedAccessView(5, labelVa);
		cmd->SetComputeRootUnorderedAccessView(6, cellCountVa);
		cmd->SetComputeRootUnorderedAccessView(7, cellPrefixVa);
		cmd->SetComputeRootUnorderedAccessView(8, scratchVec3->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(9, scratchTensorDiag->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(10, scratchTensorOffdiag->GetGPUVirtualAddress());
		cmd->Dispatch(ParticleGroups, 1, 1);
		{
			D3D12_RESOURCE_BARRIER b[] = {
				CD3DX12_RESOURCE_BARRIER::UAV(scratchVec3.Get()),
				CD3DX12_RESOURCE_BARRIER::UAV(scratchTensorDiag.Get()),
				CD3DX12_RESOURCE_BARRIER::UAV(scratchTensorOffdiag.Get()),
			};
			cmd->ResourceBarrier(_countof(b), b);
		}

		cmd->SetComputeRootSignature(commitTensorCS.rootSig.Get());
		cmd->SetPipelineState(commitTensorCS.pso.Get());
		cmd->SetComputeRootConstantBufferView(0, cbVa);
		cmd->SetComputeRootUnorderedAccessView(1, scratchVec3->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(2, scratchTensorDiag->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(3, scratchTensorOffdiag->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(4, normVa);
		cmd->SetComputeRootUnorderedAccessView(5, diagVa);
		cmd->SetComputeRootUnorderedAccessView(6, offVa);
		cmd->Dispatch(ParticleGroups, 1, 1);
		uavAll();

		// -- spacing / even distribution (same label) --
		cmd->SetComputeRootSignature(spacingLambdaCS.rootSig.Get());
		cmd->SetPipelineState(spacingLambdaCS.pso.Get());
		cmd->SetComputeRootConstantBufferView(0, cbVa);
		cmd->SetComputeRootUnorderedAccessView(1, posVa);
		cmd->SetComputeRootUnorderedAccessView(2, labelVa);
		cmd->SetComputeRootUnorderedAccessView(3, cellCountVa);
		cmd->SetComputeRootUnorderedAccessView(4, cellPrefixVa);
		cmd->SetComputeRootUnorderedAccessView(5, scratchLambda->GetGPUVirtualAddress());
		cmd->Dispatch(ParticleGroups, 1, 1);
		cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(scratchLambda.Get()));

		cmd->SetComputeRootSignature(spacingDeltaCS.rootSig.Get());
		cmd->SetPipelineState(spacingDeltaCS.pso.Get());
		cmd->SetComputeRootConstantBufferView(0, cbVa);
		cmd->SetComputeRootUnorderedAccessView(1, posVa);
		cmd->SetComputeRootUnorderedAccessView(2, labelVa);
		cmd->SetComputeRootUnorderedAccessView(3, cellCountVa);
		cmd->SetComputeRootUnorderedAccessView(4, cellPrefixVa);
		cmd->SetComputeRootUnorderedAccessView(5, scratchLambda->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(6, scratchVec3->GetGPUVirtualAddress());
		cmd->Dispatch(ParticleGroups, 1, 1);
		cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(scratchVec3.Get()));

		cmd->SetComputeRootSignature(commitPositionCS.rootSig.Get());
		cmd->SetPipelineState(commitPositionCS.pso.Get());
		cmd->SetComputeRootConstantBufferView(0, cbVa);
		cmd->SetComputeRootUnorderedAccessView(1, scratchVec3->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(2, posVa);
		cmd->Dispatch(ParticleGroups, 1, 1);
		uavAll();

		// -- anchor barrier (same label, one-sided) --
		cmd->SetComputeRootSignature(anchorBarrierCS.rootSig.Get());
		cmd->SetPipelineState(anchorBarrierCS.pso.Get());
		cmd->SetComputeRootConstantBufferView(0, cbVa);
		cmd->SetComputeRootUnorderedAccessView(1, posVa);
		cmd->SetComputeRootUnorderedAccessView(2, labelVa);
		cmd->SetComputeRootUnorderedAccessView(3, occurrenceSeedBuffer->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(4, scratchVec3->GetGPUVirtualAddress());
		cmd->Dispatch(ParticleGroups, 1, 1);
		cmd->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(scratchVec3.Get()));

		cmd->SetComputeRootSignature(commitPositionCS.rootSig.Get());
		cmd->SetPipelineState(commitPositionCS.pso.Get());
		cmd->SetComputeRootConstantBufferView(0, cbVa);
		cmd->SetComputeRootUnorderedAccessView(1, scratchVec3->GetGPUVirtualAddress());
		cmd->SetComputeRootUnorderedAccessView(2, posVa);
		cmd->Dispatch(ParticleGroups, 1, 1);
		uavAll();
	}

	void RunIterations(com_ptr<ID3D12GraphicsCommandList>& cmd, int count) {
		int n = count > 0 ? count : 0;
		for (int i = 0; i < n; i++) RunOneIteration(cmd);
	}

	// Full reseed: rebuild the test scene, the occurrence field, spawn fresh
	// particles at the boundary voxels, then relax for `iterations` steps.
	void RunReinit() {
		BuildShapeList();
		torusCb.Upload();
		UploadAequorCb();

		DX_API("reset upload allocator") uploadAllocator->Reset();
		DX_API("reset upload command list") uploadCommandList->Reset(uploadAllocator.Get(), nullptr);
		auto& cmd = uploadCommandList;

		// SpatialGrid::Build()'s internal shaders (clearGrid/countGrid/prefix
		// sum/sort/permutate) all read their buffers through descriptor
		// tables carved out of mainAlloc's GPU-visible heap -- it must be
		// bound before the first Build() call on this command list, or the
		// driver dereferences unbound table pointers.
		{
			ID3D12DescriptorHeap* heaps[] = { mainAlloc->GetHeap() };
			cmd->SetDescriptorHeaps(1, heaps);
		}

		RunOccurrenceFieldInit(cmd);
		RunSpawn(cmd);
		RunIterations(cmd, iterations);

		DX_API("close upload command list") cmd->Close();
		ID3D12CommandList* lists[] = { cmd.Get() };
		commandQueue->ExecuteCommandLists(1, lists);
		uploadFence.signal(commandQueue, ++uploadFenceValue);
		uploadFence.cpuWait();

		dataValid = true;
	}

	// Continue: no reseed, just more outer iterations on top of whatever
	// RunReinit (or an earlier Continue) already left in the particle buffers.
	void RunContinue() {
		UploadAequorCb(); // pick up any slider changes since the last run

		DX_API("reset upload allocator") uploadAllocator->Reset();
		DX_API("reset upload command list") uploadCommandList->Reset(uploadAllocator.Get(), nullptr);
		auto& cmd = uploadCommandList;

		{
			ID3D12DescriptorHeap* heaps[] = { mainAlloc->GetHeap() };
			cmd->SetDescriptorHeaps(1, heaps);
		}

		RunIterations(cmd, continueIterations);

		DX_API("close upload command list") cmd->Close();
		ID3D12CommandList* lists[] = { cmd.Get() };
		commandQueue->ExecuteCommandLists(1, lists);
		uploadFence.signal(commandQueue, ++uploadFenceValue);
		uploadFence.cpuWait();
	}

	void BuildImGui() {
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::SetNextWindowSize(ImVec2(240, 0), ImGuiCond_FirstUseEver);
		ImGui::Begin("g-Aequor Controls");
		static const char* testShapeItems[] = { "Torus", "Ellipsoid" };
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

		if (ImGui::CollapsingHeader("Constraint Parameters")) {
			ImGui::SliderFloat("Max Step", &maxStep, 0.0f, 2.0f * CellSize);
			ImGui::SliderFloat("Anchor Radius", &anchorRadius, 0.0f, 10.0f * CellSize);
			ImGui::SliderFloat("Surface Data Weight", &surfaceDataWeight, 0.0f, 5.0f);
			ImGui::SliderFloat("Surface Step Damping", &surfaceStepDamping, 0.0f, 1.0f);
			ImGui::SliderFloat("Min System Diagonal", &minSystemDiagonal, 0.0f, 1.0f);
			ImGui::SliderFloat("Quadric Consistency Weight", &quadricConsistencyWeight, 0.0f, 1.0f);
			ImGui::SliderFloat("Max Tensor Frobenius", &maxTensorFrobenius, 0.0f, 20.0f);
			ImGui::SliderFloat("Spacing Epsilon", &spacingEpsilon, 0.0f, 1000.0f);
			ImGui::SliderFloat("Residual Scale", &residualScale, 0.01f, 5.0f);
			ImGui::SliderFloat("Normal Power", &normalPower, 0.0f, 16.0f);
			ImGui::SliderFloat("Min Normal Dot", &minNormalDot, -1.0f, 1.0f);
			ImGui::SliderFloat("Max Residual", &maxResidual, 0.0f, 10.0f);
			ImGui::Checkbox("Use Residual Normalization", &useResidualNormalization);
			ImGui::SliderFloat("Foot Gate Near", &footGateNear, 0.0f, 10.0f * CellSize);
			ImGui::SliderFloat("Foot Gate Far", &footGateFar, 0.0f, 10.0f * CellSize);
		}

		if (dataValid) {
			ImGui::Separator();
			ImGui::Checkbox("Show Foot Quadrics", &showFootQuadrics);
			ImGui::SliderInt("Quadric Patch Stride", &quadricPatchStride, (int)QuadricPatchMinStride, (int)MaxParticles);
			ImGui::SliderInt("Quadric Patch Offset", &quadricPatchOffset, 0, (int)MaxParticles - 1);
			ImGui::SliderFloat("Quadric Patch Radius", &quadricPatchRadius, 0.1f, 10.0f * CellSize);
		}

		ImGui::Text("%.1f FPS", ImGui::GetIO().Framerate);
		ImGui::End();

		ImGui::Render();
		ID3D12DescriptorHeap* imguiHeaps[] = { imguiSrvHeap.Get() };
		commandList->SetDescriptorHeaps(1, imguiHeaps);
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.Get());
	}

public:
	AequorApp() : SimpleApp() {}

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

		// Created here (not LoadAssets, where g-BCC's constant buffers live)
		// because SpatialGrid::Create below bakes aequorCb's GPU VA into its
		// internal ComputeShader wrappers at construction time -- it must
		// already be a valid allocation by then.
		frameCb.CreateResources(device.Get());
		aequorCb.CreateResources(device.Get());
		torusCb.CreateResources(device.Get());

		D3D12_DESCRIPTOR_HEAP_DESC imguiHeapDesc = {};
		imguiHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		imguiHeapDesc.NumDescriptors = 1;
		imguiHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		DX_API("create imgui srv heap")
			device->CreateDescriptorHeap(&imguiHeapDesc, IID_PPV_ARGS(imguiSrvHeap.GetAddressOf()));

		mainAlloc = DescriptorAllocator::Create(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 64, true);
		staticAlloc = DescriptorAllocator::Create(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 64, false);

		for (uint f = 0; f < PF_COUNT; f++) {
			std::wstring frontName = std::wstring(aequorFieldNames[f]) + L"Front";
			std::wstring backName  = std::wstring(aequorFieldNames[f]) + L"Back";
			particleFields[f] = DoubleBufferGpuBuffer::Create(
				device.Get(), MaxParticles, aequorFieldStrides[f],
				frontName.c_str(), backName.c_str(),
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS, *staticAlloc);
		}

		spatialGrid = SpatialGrid::Create(device.Get(), GridRes * GridRes * GridRes, MaxParticles,
			*mainAlloc, *staticAlloc, aequorCb.GetGPUVirtualAddress(), particleFields);

		scratchVec3          = CreateRawUavBuffer(device.Get(), (UINT64)MaxParticles * sizeof(float) * 3, L"scratchVec3");
		scratchTensorDiag    = CreateRawUavBuffer(device.Get(), (UINT64)MaxParticles * sizeof(float) * 3, L"scratchTensorDiag");
		scratchTensorOffdiag = CreateRawUavBuffer(device.Get(), (UINT64)MaxParticles * sizeof(float) * 3, L"scratchTensorOffdiag");
		scratchLambda         = CreateRawUavBuffer(device.Get(), (UINT64)MaxParticles * sizeof(float), L"scratchLambda");
		counterBuffer          = CreateRawUavBuffer(device.Get(), sizeof(UINT), L"counterBuffer");
		rasterLabelBuffer     = CreateRawUavBuffer(device.Get(), (UINT64)GridRes * GridRes * GridRes * sizeof(UINT), L"rasterLabelBuffer");
		occ0Buffer             = CreateRawUavBuffer(device.Get(), (UINT64)GridRes * GridRes * GridRes * sizeof(UINT) * 2, L"occ0Buffer");
		occ1Buffer             = CreateRawUavBuffer(device.Get(), (UINT64)GridRes * GridRes * GridRes * sizeof(UINT) * 2, L"occ1Buffer");
		occurrenceSeedBuffer   = CreateRawUavBuffer(device.Get(), (UINT64)GridRes * GridRes * GridRes * SparseLabelCount * sizeof(UINT) * 2, L"occurrenceSeedBuffer");
		quadricPatchSegments   = CreateRawUavBuffer(device.Get(), (UINT64)MaxQuadricPatches * QuadricPatchSegmentsPerNode * sizeof(float) * 8, L"quadricPatchSegments");

		rasterLabelCS.createResources(device, "Shaders/rasterLabelCS.cso");
		occSeedCS.createResources(device, "Shaders/occSeedCS.cso");
		occStepCS.createResources(device, "Shaders/occStepCS.cso");
		occFinalizeCS.createResources(device, "Shaders/occFinalizeCS.cso");
		clearParticlesCS.createResources(device, "Shaders/clearParticlesCS.cso");
		spawnCS.createResources(device, "Shaders/spawnCS.cso");
		surfaceConstraintCS.createResources(device, "Shaders/surfaceConstraintCS.cso");
		quadricConsistencyCS.createResources(device, "Shaders/quadricConsistencyCS.cso");
		spacingLambdaCS.createResources(device, "Shaders/spacingLambdaCS.cso");
		spacingDeltaCS.createResources(device, "Shaders/spacingDeltaCS.cso");
		anchorBarrierCS.createResources(device, "Shaders/anchorBarrierCS.cso");
		commitPositionCS.createResources(device, "Shaders/commitPositionCS.cso");
		commitTensorCS.createResources(device, "Shaders/commitTensorCS.cso");
		copyParticlesCS.createResources(device, "Shaders/copyParticlesCS.cso");
		quadricPatchBuildCS.createResources(device, "Shaders/quadricPatchBuildCS.cso");

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
			com_ptr<ID3DBlob> vs = Egg::Shader::LoadCso("Shaders/particlePointVS.cso");
			com_ptr<ID3DBlob> ps = Egg::Shader::LoadCso("Shaders/particlePointPS.cso");
			particlePointRootSig = Egg::Shader::LoadRootSignature(device.Get(), vs.Get());

			D3D12_BLEND_DESC alphaBlend = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			alphaBlend.RenderTarget[0].BlendEnable    = TRUE;
			alphaBlend.RenderTarget[0].SrcBlend       = D3D12_BLEND_SRC_ALPHA;
			alphaBlend.RenderTarget[0].DestBlend      = D3D12_BLEND_INV_SRC_ALPHA;
			alphaBlend.RenderTarget[0].BlendOp        = D3D12_BLEND_OP_ADD;
			alphaBlend.RenderTarget[0].SrcBlendAlpha  = D3D12_BLEND_ONE;
			alphaBlend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
			alphaBlend.RenderTarget[0].BlendOpAlpha   = D3D12_BLEND_OP_ADD;

			D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
			psoDesc.pRootSignature = particlePointRootSig.Get();
			psoDesc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
			psoDesc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
			psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
			psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			psoDesc.DepthStencilState.DepthEnable = TRUE;
			// ALWAYS, not LESS: particles sit almost exactly on the same
			// surface the background raymarch already draws, so depth-testing
			// them against it turns into exactly the z-fighting call the user
			// diagnosed -- and standard perspective depth-buffer precision
			// loss at distance makes that fight increasingly one-sided the
			// farther a point is from the camera, so particles on the far
			// side of the torus (from the initial camera) almost always lost
			// and stayed hidden behind the (simulation-independent, always
			// static) background -- looking exactly like "that half never
			// moves" regardless of what the sim was actually doing there.
			// Particles are the thing being inspected here, so always draw
			// them on top rather than physically-correct occlusion.
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
			DX_API("create particle point PSO")
				device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(particlePointPso.GetAddressOf()));
		}

		{
			com_ptr<ID3DBlob> vs = Egg::Shader::LoadCso("Shaders/quadricPatchLineVS.cso");
			com_ptr<ID3DBlob> ps = Egg::Shader::LoadCso("Shaders/quadricPatchLinePS.cso");
			quadricPatchLineRootSig = Egg::Shader::LoadRootSignature(device.Get(), vs.Get());

			D3D12_BLEND_DESC alphaBlend = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			alphaBlend.RenderTarget[0].BlendEnable    = TRUE;
			alphaBlend.RenderTarget[0].SrcBlend       = D3D12_BLEND_SRC_ALPHA;
			alphaBlend.RenderTarget[0].DestBlend      = D3D12_BLEND_INV_SRC_ALPHA;
			alphaBlend.RenderTarget[0].BlendOp        = D3D12_BLEND_OP_ADD;
			alphaBlend.RenderTarget[0].SrcBlendAlpha  = D3D12_BLEND_ONE;
			alphaBlend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
			alphaBlend.RenderTarget[0].BlendOpAlpha   = D3D12_BLEND_OP_ADD;

			D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
			psoDesc.pRootSignature = quadricPatchLineRootSig.Get();
			psoDesc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
			psoDesc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
			psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
			psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			psoDesc.DepthStencilState.DepthEnable = TRUE;
			// ALWAYS, same reasoning as particlePointPso above -- these lines
			// sit right on the particle surface too.
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
			DX_API("create quadric patch line PSO")
				device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(quadricPatchLinePso.GetAddressOf()));
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
		float3 center(0.0f, 0.0f, 0.0f);
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
		}

		PopulateCommandList();

		ID3D12CommandList* cLists[] = { commandList.Get() };
		commandQueue->ExecuteCommandLists(1, cLists);

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

		commandList->SetGraphicsRootSignature(raymarchRootSig.Get());
		commandList->SetGraphicsRootConstantBufferView(0, frameCb.GetGPUVirtualAddress());
		commandList->SetGraphicsRootConstantBufferView(1, torusCb.GetGPUVirtualAddress());
		commandList->SetPipelineState(raymarchPso.Get());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->DrawInstanced(3, 1, 0, 0);

		if (dataValid) {
			commandList->SetGraphicsRootSignature(particlePointRootSig.Get());
			commandList->SetGraphicsRootConstantBufferView(0, frameCb.GetGPUVirtualAddress());
			commandList->SetGraphicsRootUnorderedAccessView(1, particleFields[PF_POSITION]->getFront()->Get()->GetGPUVirtualAddress());
			commandList->SetGraphicsRootUnorderedAccessView(2, particleFields[PF_LABEL]->getFront()->Get()->GetGPUVirtualAddress());
			commandList->SetPipelineState(particlePointPso.Get());
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			commandList->DrawInstanced(4, MaxParticles, 0, 0);
		}

		// -- quadric-patch wireframe overlay: rebuilds the (small, fixed-
		// capacity) segment buffer fresh every frame directly on this
		// graphics command list, same as g-BCC's equivalent overlay. --
		if (dataValid && showFootQuadrics) {
			uint stride = (quadricPatchStride > (int)QuadricPatchMinStride) ? (uint)quadricPatchStride : QuadricPatchMinStride;
			uint offset = (quadricPatchOffset > 0) ? (uint)quadricPatchOffset : 0u;
			uint numSelected = 0;
			if (offset < MaxParticles) {
				numSelected = (MaxParticles - offset + stride - 1) / stride;
				if (numSelected > MaxQuadricPatches) numSelected = MaxQuadricPatches;
			}

			if (numSelected > 0) {
				float radius = quadricPatchRadius;
				uint radiusBits = *reinterpret_cast<uint*>(&radius);
				uint dispatchGroups = (numSelected + 63) / 64;

				commandList->SetComputeRootSignature(quadricPatchBuildCS.rootSig.Get());
				commandList->SetPipelineState(quadricPatchBuildCS.pso.Get());
				commandList->SetComputeRootConstantBufferView(0, aequorCb.GetGPUVirtualAddress());
				commandList->SetComputeRoot32BitConstant(1, offset, 0);
				commandList->SetComputeRoot32BitConstant(1, stride, 1);
				commandList->SetComputeRoot32BitConstant(1, numSelected, 2);
				commandList->SetComputeRoot32BitConstant(1, radiusBits, 3);
				commandList->SetComputeRootUnorderedAccessView(2, particleFields[PF_POSITION]->getFront()->Get()->GetGPUVirtualAddress());
				commandList->SetComputeRootUnorderedAccessView(3, particleFields[PF_NORMAL]->getFront()->Get()->GetGPUVirtualAddress());
				commandList->SetComputeRootUnorderedAccessView(4, particleFields[PF_TENSOR_DIAG]->getFront()->Get()->GetGPUVirtualAddress());
				commandList->SetComputeRootUnorderedAccessView(5, particleFields[PF_TENSOR_OFFDIAG]->getFront()->Get()->GetGPUVirtualAddress());
				commandList->SetComputeRootUnorderedAccessView(6, particleFields[PF_LABEL]->getFront()->Get()->GetGPUVirtualAddress());
				commandList->SetComputeRootUnorderedAccessView(7, quadricPatchSegments->GetGPUVirtualAddress());
				commandList->Dispatch(dispatchGroups, 1, 1);

				commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(quadricPatchSegments.Get()));

				commandList->SetGraphicsRootSignature(quadricPatchLineRootSig.Get());
				commandList->SetGraphicsRootConstantBufferView(0, frameCb.GetGPUVirtualAddress());
				commandList->SetGraphicsRootUnorderedAccessView(1, quadricPatchSegments->GetGPUVirtualAddress());
				commandList->SetPipelineState(quadricPatchLinePso.Get());
				commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				commandList->DrawInstanced(4, numSelected * QuadricPatchSegmentsPerNode, 0, 0);
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
		frameCb.data.raymarchParams    = float4((float)MaxMarchSteps, MaxMarchDist, 0.0f, 0.0f);
		frameCb.data.pointParams       = float4(viewPort.Width, viewPort.Height, PointRadiusPx, 0.0f);
		frameCb.Upload();
	}

	virtual void ProcessMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override {
		if (camera) camera->ProcessMessage(hWnd, uMsg, wParam, lParam);

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
		aequorCb.ReleaseResources();
		torusCb.ReleaseResources();
		scratchVec3.Reset();
		scratchTensorDiag.Reset();
		scratchTensorOffdiag.Reset();
		scratchLambda.Reset();
		counterBuffer.Reset();
		rasterLabelBuffer.Reset();
		occ0Buffer.Reset();
		occ1Buffer.Reset();
		occurrenceSeedBuffer.Reset();
		quadricPatchSegments.Reset();
		imguiSrvHeap.Reset();
		raymarchRootSig.Reset();
		raymarchPso.Reset();
		particlePointRootSig.Reset();
		particlePointPso.Reset();
		quadricPatchLineRootSig.Reset();
		quadricPatchLinePso.Reset();
		Egg::SimpleApp::ReleaseResources();
	}
};
