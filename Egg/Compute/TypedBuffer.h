#pragma once
#include "../Common.h"
#include <string>

namespace Egg {
	namespace Compute {

		class TypedBuffer {
			com_ptr<ID3D12Resource>     buffer;
			com_ptr<ID3D12Resource>     uploadBuffer;
			com_ptr<ID3D12Resource>     readbackBuffer;
			uint    numElements;
			DXGI_FORMAT format;
			uint    elementByteSize;
			std::wstring debugName;

			static uint getElementByteSize(DXGI_FORMAT fmt) {
				switch (fmt) {
				case DXGI_FORMAT_R32G32B32A32_UINT:
				case DXGI_FORMAT_R32G32B32A32_FLOAT: return 16;
				case DXGI_FORMAT_R32G32_UINT:
				case DXGI_FORMAT_R32G32_FLOAT:       return 8;
				case DXGI_FORMAT_R32_UINT:
				case DXGI_FORMAT_R32_FLOAT:          return 4;
				default:                             return 0;
				}
			}

		public:
			TypedBuffer(
				std::wstring debugName,
				uint numElements,
				DXGI_FORMAT format = DXGI_FORMAT_R32G32B32A32_UINT)
				: debugName(debugName), numElements(numElements), format(format),
				  elementByteSize(getElementByteSize(format)) {}

			void createResources(com_ptr<ID3D12Device> device,
				const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
			{
				uint totalBytes = numElements * elementByteSize;

				const D3D12_HEAP_PROPERTIES defaultHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
				const D3D12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(
					totalBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
				DX_API("create typed buffer")
					device->CreateCommittedResource(
						&defaultHeapProps,
						D3D12_HEAP_FLAG_NONE,
						&bufferDesc,
						D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
						nullptr,
						IID_PPV_ARGS(buffer.ReleaseAndGetAddressOf()));
				buffer->SetName(debugName.c_str());

				CD3DX12_HEAP_PROPERTIES rbHeapProps(D3D12_HEAP_TYPE_READBACK);
				const D3D12_RESOURCE_DESC tempDesc = CD3DX12_RESOURCE_DESC::Buffer(totalBytes);
				DX_API("create typed buffer readback")
					device->CreateCommittedResource(
						&rbHeapProps,
						D3D12_HEAP_FLAG_NONE,
						&tempDesc,
						D3D12_RESOURCE_STATE_COPY_DEST,
						nullptr,
						IID_PPV_ARGS(readbackBuffer.ReleaseAndGetAddressOf()));
				readbackBuffer->SetName((debugName + L" [READBACK]").c_str());

				CD3DX12_HEAP_PROPERTIES upHeapProps(D3D12_HEAP_TYPE_UPLOAD);
				DX_API("create typed buffer upload")
					device->CreateCommittedResource(
						&upHeapProps,
						D3D12_HEAP_FLAG_NONE,
						&tempDesc,
						D3D12_RESOURCE_STATE_GENERIC_READ,
						nullptr,
						IID_PPV_ARGS(uploadBuffer.ReleaseAndGetAddressOf()));
				uploadBuffer->SetName((debugName + L" [UPLOAD]").c_str());

				D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
				uavDesc.Format                      = format;
				uavDesc.ViewDimension               = D3D12_UAV_DIMENSION_BUFFER;
				uavDesc.Buffer.FirstElement         = 0;
				uavDesc.Buffer.NumElements          = numElements;
				uavDesc.Buffer.StructureByteStride  = 0;
				uavDesc.Buffer.CounterOffsetInBytes = 0;
				uavDesc.Buffer.Flags                = D3D12_BUFFER_UAV_FLAG_NONE;
				device->CreateUnorderedAccessView(buffer.Get(), nullptr, &uavDesc, handle);
			}

			void copyBack(com_ptr<ID3D12GraphicsCommandList> commandList) {
				D3D12_RESOURCE_BARRIER b = {};
				b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				b.Transition.pResource   = buffer.Get();
				b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				b.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
				b.Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_SOURCE;
				commandList->ResourceBarrier(1, &b);
				commandList->CopyResource(readbackBuffer.Get(), buffer.Get());
				b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
				b.Transition.StateAfter  = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
				commandList->ResourceBarrier(1, &b);
			}

			void* mapReadback() {
				D3D12_RANGE range{ 0, (SIZE_T)numElements * elementByteSize };
				void* pData = nullptr;
				readbackBuffer->Map(0, &range, &pData);
				return pData;
			}

			void unmapReadback() {
				D3D12_RANGE emptyRange{ 0, 0 };
				readbackBuffer->Unmap(0, &emptyRange);
			}

			void releaseResources() {
				buffer.Reset();
				uploadBuffer.Reset();
				readbackBuffer.Reset();
			}
		};
	}
}
