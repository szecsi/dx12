#pragma once
#include "Egg/Common.h"
#include "Fence.h"

class FenceChain {
	uint64_t nextValueToSignal;
	std::vector<Fence> fences;
public:
	FenceChain() {
	}

	void createResources(com_ptr<ID3D12Device> device, uint swapChainBackBufferCount, D3D12_FENCE_FLAGS flags = D3D12_FENCE_FLAG_SHARED) {
		nextValueToSignal = 1;
		fences.resize(swapChainBackBufferCount);
		for (auto& f : fences) {
			f.createResources(device, flags);
		}
	}

	void signal(com_ptr<ID3D12CommandQueue> queue, uint64_t swapChainBackBufferIndex) {
		fences[swapChainBackBufferIndex].signal(queue, nextValueToSignal);
		nextValueToSignal++;
	}

	void cpuWait(uint64_t swapChainBackBufferIndex) {
		fences[swapChainBackBufferIndex].cpuWait();
	}

	void gpuWait(com_ptr<ID3D12CommandQueue> queue, uint64_t swapChainBackBufferIndex) {
		fences[swapChainBackBufferIndex].gpuWait(queue);
	}

	void destroy() {
		for (auto& f : fences) {
			f.destroy();
		}
	}
};