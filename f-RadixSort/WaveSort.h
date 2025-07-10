#pragma once
#include "ComputeShader.h"
#include "RawBuffer.h"

class WaveSort {
	ComputeShader localSortInPlace;
	ComputeShader localSort;
	ComputeShader merge;
	D3D12_GPU_DESCRIPTOR_HANDLE uavHandle; //uavHeap->GetGPUDescriptorHandleForHeapStart()
	uint uavOffset;
	bool interleaveBits;

	D3D12_RESOURCE_BARRIER uavBarriers[5];

public:
	void creaseResources(ComputeShader localSortInPlace, ComputeShader localSort, ComputeShader merge, CD3DX12_GPU_DESCRIPTOR_HANDLE uavHandle, uint uavOffset, uint dhIncrSize,
		const std::vector<RawBuffer>& buffers, bool interleaveBits = false)
	{
		this->localSort = localSort;
		this->localSortInPlace = localSortInPlace;
		this->merge = merge;
		this->uavHandle = uavHandle.Offset(uavOffset, dhIncrSize);
		this->uavOffset = uavOffset;
		this->interleaveBits = interleaveBits;

		for (uint i = 0; i < 5; i++) {
			uavBarriers[i] = buffers[uavOffset + i].uavBarrier();
		}
	}

	void populate(com_ptr<ID3D12GraphicsCommandList> computeCommandList) {
		localSortInPlace.setup(computeCommandList, uavHandle);
		computeCommandList->SetComputeRoot32BitConstant(0, interleaveBits? 0x01160b00: 0x03020100, 0);
		computeCommandList->Dispatch(32, 1, 1);
		computeCommandList->ResourceBarrier(3, uavBarriers);

		merge.setup(computeCommandList, uavHandle);
		computeCommandList->SetComputeRoot32BitConstant(0, interleaveBits ? 0x01160b00 : 0x03020100, 0);
		computeCommandList->Dispatch(32, 1, 1);
		computeCommandList->ResourceBarrier(2, &uavBarriers[3]);

		localSort.setup(computeCommandList, uavHandle);
		computeCommandList->SetComputeRoot32BitConstant(0, interleaveBits ? 0x0d02170c : 0x07060504, 0);
		computeCommandList->Dispatch(32, 1, 1);
		computeCommandList->ResourceBarrier(3, uavBarriers);

		merge.setup(computeCommandList, uavHandle);
		computeCommandList->SetComputeRoot32BitConstant(0, interleaveBits ? 0x0d02170c : 0x07060504, 0);
		computeCommandList->Dispatch(32, 1, 1);
		computeCommandList->ResourceBarrier(2, &uavBarriers[3]);

		localSort.setup(computeCommandList, uavHandle);
		computeCommandList->SetComputeRoot32BitConstant(0, interleaveBits ? 0x190e0318 : 0x0b0a0908, 0);
		computeCommandList->Dispatch(32, 1, 1);
		computeCommandList->ResourceBarrier(3, uavBarriers);

		merge.setup(computeCommandList, uavHandle);
		computeCommandList->SetComputeRoot32BitConstant(0, interleaveBits ? 0x190e0318 : 0x0b0a0908, 0);
		computeCommandList->Dispatch(32, 1, 1);
		computeCommandList->ResourceBarrier(2, &uavBarriers[3]);

		localSort.setup(computeCommandList, uavHandle);
		computeCommandList->SetComputeRoot32BitConstant(0, interleaveBits ? 0x051a0f04 : 0x0f0e0d0c, 0);
		computeCommandList->Dispatch(32, 1, 1);
		computeCommandList->ResourceBarrier(3, uavBarriers);

		merge.setup(computeCommandList, uavHandle);
		computeCommandList->SetComputeRoot32BitConstant(0, interleaveBits ? 0x051a0f04 : 0x0f0e0d0c, 0);
		computeCommandList->Dispatch(32, 1, 1);
		computeCommandList->ResourceBarrier(2, &uavBarriers[3]);

		localSort.setup(computeCommandList, uavHandle);
		computeCommandList->SetComputeRoot32BitConstant(0, interleaveBits ? 0x11061b10 : 0x13121110, 0);
		computeCommandList->Dispatch(32, 1, 1);
		computeCommandList->ResourceBarrier(3, uavBarriers);

		merge.setup(computeCommandList, uavHandle);
		computeCommandList->SetComputeRoot32BitConstant(0, interleaveBits ? 0x11061b10 : 0x13121110, 0);
		computeCommandList->Dispatch(32, 1, 1);
		computeCommandList->ResourceBarrier(2, &uavBarriers[3]);

		localSort.setup(computeCommandList, uavHandle);
		computeCommandList->SetComputeRoot32BitConstant(0, interleaveBits ? 0x1d12071c : 0x17161514, 0);
		computeCommandList->Dispatch(32, 1, 1);
		computeCommandList->ResourceBarrier(3, uavBarriers);

		merge.setup(computeCommandList, uavHandle);
		computeCommandList->SetComputeRoot32BitConstant(0, interleaveBits ? 0x1d12071c : 0x17161514, 0);
		computeCommandList->Dispatch(32, 1, 1);
		computeCommandList->ResourceBarrier(2, &uavBarriers[3]);

		localSort.setup(computeCommandList, uavHandle);
		computeCommandList->SetComputeRoot32BitConstant(0, interleaveBits ? 0x091e1308 : 0x1b1a1918, 0);
		computeCommandList->Dispatch(32, 1, 1);
		computeCommandList->ResourceBarrier(3, uavBarriers);

		merge.setup(computeCommandList, uavHandle);
		computeCommandList->SetComputeRoot32BitConstant(0, interleaveBits ? 0x091e1308 : 0x1b1a1918, 0);
		computeCommandList->Dispatch(32, 1, 1);
		computeCommandList->ResourceBarrier(2, &uavBarriers[3]);

		localSort.setup(computeCommandList, uavHandle);
		computeCommandList->SetComputeRoot32BitConstant(0, interleaveBits ? 0x150a1f14 : 0x1f1e1d1c, 0);
		computeCommandList->Dispatch(32, 1, 1);
		computeCommandList->ResourceBarrier(3, uavBarriers);

		merge.setup(computeCommandList, uavHandle);
		computeCommandList->SetComputeRoot32BitConstant(0, interleaveBits ? 0x150a1f14 : 0x1f1e1d1c, 0);
		computeCommandList->Dispatch(32, 1, 1);
		computeCommandList->ResourceBarrier(2, &uavBarriers[3]);
	}

//	void populate(com_ptr<ID3D12GraphicsCommandList> computeCommandList) {
//		localSort.setup(computeCommandList, uavHandle);
//
//		computeCommandList->SetComputeRoot32BitConstant(0, interleaveBits? 0x01160b00: 0x03020100, 0);
//		computeCommandList->Dispatch(32, 1, 1);
//		computeCommandList->ResourceBarrier(3, uavBarriers);
//		computeCommandList->SetComputeRoot32BitConstant(0, interleaveBits ? 0x0d02170c : 0x07060504, 0);
//		computeCommandList->Dispatch(32, 1, 1);
//		computeCommandList->ResourceBarrier(3, uavBarriers);
//		computeCommandList->SetComputeRoot32BitConstant(0, interleaveBits ? 0x190e0318 : 0x0b0a0908, 0);
//		computeCommandList->Dispatch(32, 1, 1);
//		computeCommandList->ResourceBarrier(3, uavBarriers);
//		computeCommandList->SetComputeRoot32BitConstant(0, interleaveBits ? 0x051a0f04 : 0x0f0e0d0c, 0);
//		computeCommandList->Dispatch(32, 1, 1);
//		computeCommandList->ResourceBarrier(3, uavBarriers);
//		computeCommandList->SetComputeRoot32BitConstant(0, interleaveBits ? 0x11061b10 : 0x13121110, 0);
//		computeCommandList->Dispatch(32, 1, 1);
//		computeCommandList->ResourceBarrier(3, uavBarriers);
//		computeCommandList->SetComputeRoot32BitConstant(0, interleaveBits ? 0x1d12071c : 0x17161514, 0);
//		computeCommandList->Dispatch(32, 1, 1);
//		computeCommandList->ResourceBarrier(3, uavBarriers);
//		computeCommandList->SetComputeRoot32BitConstant(0, interleaveBits ? 0x091e1308 : 0x1b1a1918, 0);
//		computeCommandList->Dispatch(32, 1, 1);
//		computeCommandList->ResourceBarrier(3, uavBarriers);
//		computeCommandList->SetComputeRoot32BitConstant(0, interleaveBits ? 0x150a1f14 : 0x1f1e1d1c, 0);
//		computeCommandList->Dispatch(32, 1, 1);
//		computeCommandList->ResourceBarrier(3, uavBarriers);
//
//		merge.setup(computeCommandList, uavHandle);
//		computeCommandList->Dispatch(1, 1, 1);
//		computeCommandList->ResourceBarrier(2, &uavBarriers[3]);
//	}
};