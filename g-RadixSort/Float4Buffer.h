#pragma once
#include "Egg/Common.h"
#include <d3d11on12.h>
#include <string>
#include "Egg/Math/Math.h"

class Float4Buffer {
	com_ptr<ID3D12Resource>		buffer;
	com_ptr<ID3D12Resource>     uploadBuffer;
	com_ptr<ID3D12Resource>		readbackBuffer;
	com_ptr<ID3D11Resource>		wrappedBuffer;
	uint bufferFloat4Size;
	std::wstring debugName;
	bool sharedWithD3D11;
public:
	com_ptr<ID3D11Resource>		getWrappedBuffer() { return wrappedBuffer; }

	Float4Buffer(
		std::wstring debugName,
		bool sharedWithD3D11 = false,
		uint bufferFloat4Size = 32 * 32 * 32) :debugName(debugName), sharedWithD3D11(sharedWithD3D11), bufferFloat4Size(bufferFloat4Size) {
	}

	void createResources(com_ptr<ID3D12Device> device,
		com_ptr<ID3D11On12Device> device11on12,
		const D3D12_CPU_DESCRIPTOR_HANDLE& handle
	) {
		const D3D12_HEAP_PROPERTIES defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		const D3D12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(4 * 4 * bufferFloat4Size,
			D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 0);
		DX_API("commited resource")
			device->CreateCommittedResource(
				&defaultHeapProperties,
				//not needed for d3d11 interop, do not believe bogus warning: sharedWithD3D11?D3D12_HEAP_FLAG_SHARED : D3D12_HEAP_FLAG_NONE, 
				D3D12_HEAP_FLAG_NONE,
				&bufferDesc,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				nullptr,
				IID_PPV_ARGS(buffer.ReleaseAndGetAddressOf()));
		buffer->SetName(debugName.c_str());

		CD3DX12_HEAP_PROPERTIES rbheapProps(D3D12_HEAP_TYPE_READBACK);

		D3D12_RESOURCE_ALLOCATION_INFO info = {};
		info.SizeInBytes = 4 * 4 * bufferFloat4Size;
		info.Alignment = 0;
		const D3D12_RESOURCE_DESC tempBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(info);
		DX_API("readback resource")
			device->CreateCommittedResource(
				&rbheapProps,
				D3D12_HEAP_FLAG_NONE,
				&tempBufferDesc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(readbackBuffer.ReleaseAndGetAddressOf()));
		readbackBuffer->SetName((debugName + L" [READBACK]").c_str());

		CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);

		DX_API("upload resource")
			device->CreateCommittedResource(
				&heapProps,
				D3D12_HEAP_FLAG_NONE,
				&tempBufferDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(uploadBuffer.ReleaseAndGetAddressOf()));
		uploadBuffer->SetName((debugName + L" [UPLOAD]").c_str());

		if (sharedWithD3D11) {
			D3D11_RESOURCE_FLAGS d3d11Flags;
			d3d11Flags.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
			d3d11Flags.MiscFlags = 0; // no structured
			d3d11Flags.CPUAccessFlags = 0;
			d3d11Flags.StructureByteStride = 0;
			DX_API("wrap 12 uav buffer for d3d11")
				device11on12->CreateWrappedResource(
					buffer.Get(),
					&d3d11Flags,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
					IID_PPV_ARGS(wrappedBuffer.GetAddressOf())
				);
			device11on12->ReleaseWrappedResources(wrappedBuffer.GetAddressOf(), 1);
		}

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
		uavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.CounterOffsetInBytes = 0;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
		uavDesc.Buffer.NumElements = bufferFloat4Size;
		uavDesc.Buffer.StructureByteStride = 0;
		// create uav
		device->CreateUnorderedAccessView(buffer.Get(), nullptr, &uavDesc, handle);
	}

	void releaseResources() {
		buffer.Reset();
		uploadBuffer.Reset();
		readbackBuffer.Reset();
		wrappedBuffer.Reset();
	}

	void copyBack(com_ptr<ID3D12GraphicsCommandList> commandList) {
		barrier(commandList, buffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
		commandList->CopyResource(readbackBuffer.Get(), buffer.Get());
		barrier(commandList, buffer, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	}

	unsigned int* mapReadback() {
		D3D12_RANGE readbackBufferRange{ 0, 4 * 32 * 32 * 32 };
		unsigned int* pReadbackBufferData;
		readbackBuffer->Map
		(
			0,
			&readbackBufferRange,
			reinterpret_cast<void**>(&pReadbackBufferData)
		);
		return pReadbackBufferData;
	}
	void unmapReadback() {
		D3D12_RANGE emptyRange{ 0, 0 };
		readbackBuffer->Unmap
		(
			0,
			&emptyRange
		);
	}

	void fillRandom() {
		void* pData;
		CD3DX12_RANGE range(0, bufferFloat4Size);
		uploadBuffer->Map(0, &range, &pData);
		Egg::Math::Float4* m_arrayDataBegin = reinterpret_cast<Egg::Math::Float4*>(pData);
		Egg::Math::Float4* m_arrayDataEnd = m_arrayDataBegin + bufferFloat4Size;

		for (auto ip = m_arrayDataBegin; ip < m_arrayDataEnd; ip++) {
			ip->Random();
		}

		uploadBuffer->Unmap(0, &range);
	}

	static void barrier(com_ptr<ID3D12GraphicsCommandList> pCmdList, com_ptr<ID3D12Resource> pResource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after, D3D12_RESOURCE_BARRIER_FLAGS flags = D3D12_RESOURCE_BARRIER_FLAG_NONE)
	{
		D3D12_RESOURCE_BARRIER barrierDesc = {};

		barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrierDesc.Flags = flags;
		barrierDesc.Transition.pResource = pResource.Get();
		barrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrierDesc.Transition.StateBefore = before;
		barrierDesc.Transition.StateAfter = after;

		pCmdList->ResourceBarrier(1, &barrierDesc);
	}

	void upload(com_ptr<ID3D12GraphicsCommandList> commandList) {
		barrier(commandList, buffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);

		commandList->CopyResource(buffer.Get(), uploadBuffer.Get());

		barrier(commandList, buffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	}

	D3D12_RESOURCE_BARRIER uavBarrier() const {
		D3D12_RESOURCE_BARRIER b;
		b.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		b.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		b.UAV.pResource = buffer.Get();
		return b;
	}

};
