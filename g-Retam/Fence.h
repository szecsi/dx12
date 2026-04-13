#pragma once
#include "Egg/Common.h"

class Fence {
	com_ptr<ID3D12Fence>						fence;
	uint64_t                                    valueSignalled; // The value we have signalled, and can be waited on.
	Microsoft::WRL::Wrappers::Event             event;
public:
	void createResources(com_ptr<ID3D12Device> device, D3D12_FENCE_FLAGS flags = D3D12_FENCE_FLAG_SHARED) {
		valueSignalled = 0;
		DX_API("Fence")
			device->CreateFence(valueSignalled, flags, IID_PPV_ARGS(&fence));

		event.Attach(CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS));
	}

	void signal(com_ptr<ID3D12CommandQueue> queue, uint64_t valueToSignal) {
		queue->Signal(fence.Get(), valueToSignal);
		valueSignalled = valueToSignal;
	}

	void cpuWait() {
		if (fence->GetCompletedValue() < valueSignalled) {
			DX_API("Waiting for fence")
				fence->SetEventOnCompletion(valueSignalled, event.Get());
			WaitForSingleObject(event.Get(), INFINITE);
		}
	}

	void gpuWait(com_ptr<ID3D12CommandQueue> queue) {
		queue->Wait(fence.Get(), valueSignalled);
	}

	void destroy() {
		cpuWait();
		event.Close();
	}
};