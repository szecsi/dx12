#pragma once
#include "Egg/Compute/ComputeShader.h"
#include "Egg/Compute/RawBuffer.h"

class WaveSort {
	Egg::Compute::ComputeShader csLocalSortAlpha;
	Egg::Compute::ComputeShader csLocalSortBeta;
	Egg::Compute::ComputeShader csLocalSortGamma;
	Egg::Compute::ComputeShader csScan;
	Egg::Compute::ComputeShader csPackAlpha;
	Egg::Compute::ComputeShader csPackBeta;
	Egg::Compute::ComputeShader csPackGamma;
	D3D12_GPU_DESCRIPTOR_HANDLE uavHandle; //uavHeap->GetGPUDescriptorHandleForHeapStart()
	uint uavOffset;
	bool interleaveBits;

	D3D12_RESOURCE_BARRIER uavBarriers[5];

public:
	void creaseResources(
		Egg::Compute::ComputeShader csLocalSortAlpha,
		Egg::Compute::ComputeShader csLocalSortBeta,
		Egg::Compute::ComputeShader csLocalSortGamma,
		Egg::Compute::ComputeShader csScan,
		Egg::Compute::ComputeShader csPackAlpha,
		Egg::Compute::ComputeShader csPackBeta,
		Egg::Compute::ComputeShader csPackGamma,
		CD3DX12_GPU_DESCRIPTOR_HANDLE uavHandle, uint uavOffset, uint dhIncrSize,
		const std::vector<Egg::Compute::RawBuffer>& buffers, bool interleaveBits = false)
	{
		this->csLocalSortAlpha = csLocalSortAlpha;
		this->csLocalSortBeta = csLocalSortBeta;
		this->csLocalSortGamma = csLocalSortGamma;
		this->csScan = csScan;
		this->csPackAlpha = csPackAlpha;
		this->csPackBeta  = csPackBeta;
		this->csPackGamma = csPackGamma;

		this->uavHandle = uavHandle.Offset(uavOffset, dhIncrSize);
		this->uavOffset = uavOffset;
		this->interleaveBits = interleaveBits;

		for (uint i = 0; i < 5; i++) {
			uavBarriers[i] = buffers[uavOffset + i].uavBarrier();
		}
	}

	void populate(com_ptr<ID3D12GraphicsCommandList> computeCommandList) {
		csLocalSortAlpha.setup(computeCommandList, uavHandle);
		computeCommandList->Dispatch(1024, 1, 1);
		computeCommandList->ResourceBarrier(1, &uavBarriers[1]); // perPageBucketOffsets
		
		csScan.setup(computeCommandList, uavHandle);
		computeCommandList->Dispatch(16, 1, 1);
		computeCommandList->ResourceBarrier(2, &uavBarriers[3]); // globalBucketOffsets, ik1 (not written in this last pass, but written previously, and used in next pass)

		csPackAlpha.setup(computeCommandList, uavHandle);
		computeCommandList->Dispatch(1024, 1, 1);
		computeCommandList->ResourceBarrier(1, &uavBarriers[2]); //ik0

		for (int i = 0; i < 3; i++) {
			csLocalSortBeta.setup(computeCommandList, uavHandle);
			computeCommandList->SetComputeRoot32BitConstant(0, interleaveBits ? 0x01160b00 : 0x03020100, 0);
			computeCommandList->Dispatch(1024, 1, 1);
			computeCommandList->ResourceBarrier(1, &uavBarriers[1]); // perPageBucketOffsets

			csScan.setup(computeCommandList, uavHandle);
			computeCommandList->SetComputeRoot32BitConstant(0, interleaveBits ? 0x01160b00 : 0x03020100, 0);
			computeCommandList->Dispatch(16, 1, 1);
			computeCommandList->ResourceBarrier(2, &uavBarriers[3]); // globalBucketOffsets, ik1 (not written in this last pass, but written previously, and used in next pass)

			csPackBeta.setup(computeCommandList, uavHandle);
			computeCommandList->SetComputeRoot32BitConstant(0, interleaveBits ? 0x01160b00 : 0x03020100, 0);
			computeCommandList->Dispatch(1024, 1, 1);
			computeCommandList->ResourceBarrier(1, &uavBarriers[2]); //ik0
			
		}

		csLocalSortGamma.setup(computeCommandList, uavHandle);
		computeCommandList->Dispatch(1024, 1, 1);
		computeCommandList->ResourceBarrier(1, &uavBarriers[1]); // perPageBucketOffsets
		
		csScan.setup(computeCommandList, uavHandle);
		computeCommandList->Dispatch(16, 1, 1);
		computeCommandList->ResourceBarrier(2, &uavBarriers[3]); // globalBucketOffsets, ik1 (not written in this last pass, but written previously, and used in next pass)

		csPackGamma.setup(computeCommandList, uavHandle);
		computeCommandList->Dispatch(1024, 1, 1);
		computeCommandList->ResourceBarrier(1, &uavBarriers[2]); //ik0
		
		for (int i = 0; i < 3; i++) {
			csLocalSortBeta.setup(computeCommandList, uavHandle);
			computeCommandList->SetComputeRoot32BitConstant(0, interleaveBits ? 0x01160b00 : 0x03020100, 0);
			computeCommandList->Dispatch(1024, 1, 1);
			computeCommandList->ResourceBarrier(1, &uavBarriers[1]); // perPageBucketOffsets

			csScan.setup(computeCommandList, uavHandle);
			computeCommandList->SetComputeRoot32BitConstant(0, interleaveBits ? 0x01160b00 : 0x03020100, 0);
			computeCommandList->Dispatch(16, 1, 1);
			computeCommandList->ResourceBarrier(2, &uavBarriers[3]); // globalBucketOffsets, ik1 (not written in this last pass, but written previously, and used in next pass)

			csPackBeta.setup(computeCommandList, uavHandle);
			computeCommandList->SetComputeRoot32BitConstant(0, interleaveBits ? 0x01160b00 : 0x03020100, 0);
			computeCommandList->Dispatch(1024, 1, 1);
			computeCommandList->ResourceBarrier(1, &uavBarriers[2]); //ik0

		}

	}
};