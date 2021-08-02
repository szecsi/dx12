#pragma once

#include <Egg/SimpleApp.h>
#include <Egg/Importer.h>
#include <Egg/Math/Math.h>
#include <Egg/ConstantBuffer.hpp>
#include <thread>

#include "ConstantBufferTypes.h"

using namespace Egg::Math;

class ggl004App : public Egg::SimpleApp {
protected:
	com_ptr<ID3D12DescriptorHeap> uavHeap;

	com_ptr<ID3D12PipelineState> m_computePSO;
	com_ptr<ID3D12RootSignature> m_computeRootSignature;
	com_ptr<ID3D12PipelineState> m_mergePSO;
	com_ptr<ID3D12RootSignature> m_mergeRootSignature;
	com_ptr<ID3D12CommandAllocator> m_computeAllocator;
	com_ptr<ID3D12CommandQueue>  m_computeCommandQueue;
	com_ptr<ID3D12GraphicsCommandList> m_computeCommandList;

	unsigned int* m_arrayDataBegin;
	unsigned int* m_arrayDataEnd;

	com_ptr<ID3D12Resource>      m_arrayReadback;
	com_ptr<ID3D12Resource>      m_array[2];
	D3D12_RESOURCE_STATES		 m_stateArray[2];
	com_ptr<ID3D12Resource>      m_bucketMatrix[2];
	D3D12_RESOURCE_STATES		 m_stateBucket[2];
	com_ptr<ID3D12Resource>      m_arrayData[2]; //upload and readback
	com_ptr<ID3D12Fence>         m_renderResourceFence;  // fence used by async compute to start once it's texture has changed to unordered access
	uint64_t                     m_renderResourceFenceValue;

	com_ptr<ID3D12Fence>						m_computeFence; // fence used by the async compute shader to stall waiting for task is complete, so it can signal render when it's done
	uint64_t                                    m_computeFenceValue;
	Microsoft::WRL::Wrappers::Event             m_computeFenceEvent;
	Microsoft::WRL::Wrappers::Event             m_computeResumeSignal;

	enum ResourceBufferState : uint32_t
	{
		ResourceState_ReadyCompute,
		ResourceState_Computing,    // async is currently running on this resource buffer
		ResourceState_Computed,     // async buffer has been updated, no one is using it, moved to this state by async thread, only render will access in this state
		ResourceState_Switching,    // switching buffer from texture to unordered, from render to compute access
		ResourceState_Rendering,    // buffer is currently being used by the render system for the frame
		ResourceState_Rendered      // render frame finished for this resource. possible to switch to computing by render thread if needed
	};

	std::atomic<ResourceBufferState> m_resourceState[2];

public:
	virtual void Update(float dt, float T) override {
	}

	virtual void PopulateCommandList() override {

/*		if (m_resourceState[0] == ResourceState_Computed)			// async has finished with an update, so swap out the buffers
		{
			m_renderResourceFenceValue++;
//			EnsureResourceState(RenderIndex(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
//			m_resourceState[RenderIndex()] = ResourceState_Switching;
//			SwapRenderComputeIndex();
		}
		else if (m_resourceState[0] == ResourceState_Switching)	// the compute buffer has finished being swapped from a pixel shader view to an unordered access view
		{																		// it's now ready for the async compute thread to use
			m_resourceState[0] = ResourceState_ReadyCompute;
		}
		else if (m_resourceState[0] == ResourceState_ReadyCompute)	// the async compute thread hasn't kicked off and starting using the compute buffer
		{
			// do nothing, still waiting on async compute to actually do work
		}
		else //if (m_windowUpdated)												// need to kick off a new async compute, the user has changed the view area with the controller
		{
//			assert((m_resourceState[0] == ResourceState_ReadyCompute) || (m_resourceState[RenderIndex()] == ResourceState_Rendered));
			m_renderResourceFenceValue++;
//			EnsureResourceState(RenderIndex(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			m_resourceState[0] = ResourceState_Switching;
//			SwapRenderComputeIndex();
		}*/

		commandAllocator->Reset();
		commandList->Reset(commandAllocator.Get(), nullptr);

		if (m_resourceState[0] == ResourceState_Computed) {

//			ResourceBarrier(commandList.Get(), m_array[1].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);

			commandList->CopyResource(m_arrayReadback.Get(), m_array[1].Get());

//			ResourceBarrier(commandList.Get(), m_array[1].Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

//			ResourceBarrier(commandList.Get(), m_arrayReadback.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);

			m_resourceState[0] = ResourceState_ReadyCompute;
		}

		commandList->RSSetViewports(1, &viewPort);
		commandList->RSSetScissorRects(1, &scissorRect);

		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

		CD3DX12_CPU_DESCRIPTOR_HANDLE rHandle(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), frameIndex, rtvDescriptorHandleIncrementSize);
		CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(dsvHeap->GetCPUDescriptorHandleForHeapStart());
		commandList->OMSetRenderTargets(1, &rHandle, FALSE, &dsvHandle);

		const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
		commandList->ClearRenderTargetView(rHandle, clearColor, 0, nullptr);
		commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

//		ID3D12DescriptorHeap* descriptorHeaps[] = { uavHeap.Get() };
//		commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
//
//		commandList->SetGraphicsRootDescriptorTable(0, uavHeap->GetGPUDescriptorHandleForHeapStart());
//		commandList->Dispatch(1, 0, 0);

		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

		DX_API("Failed to close command list")
			commandList->Close();

		ID3D12CommandList* cLists[] = { commandList.Get() };
		commandQueue->ExecuteCommandLists(_countof(cLists), cLists);

		WaitForPreviousFrame();

		D3D12_RANGE readbackBufferRange{ 0, 4*32*32*32 };
		unsigned int* pReadbackBufferData;
		m_arrayReadback->Map
		(
			0,
			&readbackBufferRange,
			reinterpret_cast<void**>(&pReadbackBufferData)
		);

		// Code goes here to access the data via pReadbackBufferData.

		D3D12_RANGE emptyRange{ 0, 0 };
		m_arrayReadback->Unmap
		(
			0,
			&emptyRange
		);
	}

	/*
	Almost a render call
	*/
	void UploadResources() {
	}

	void ResourceBarrier(_In_ ID3D12GraphicsCommandList* pCmdList, _In_ ID3D12Resource* pResource, D3D12_RESOURCE_STATES Before, D3D12_RESOURCE_STATES After, D3D12_RESOURCE_BARRIER_FLAGS Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE)
	{
		D3D12_RESOURCE_BARRIER barrierDesc = {};

		barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrierDesc.Flags = Flags;
		barrierDesc.Transition.pResource = pResource;
		barrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrierDesc.Transition.StateBefore = Before;
		barrierDesc.Transition.StateAfter = After;

		pCmdList->ResourceBarrier(1, &barrierDesc);
	}

	virtual void CreateResources() override {
		Egg::SimpleApp::CreateResources();

		m_resourceState[0] = m_resourceState[1] = ResourceState_ReadyCompute;

		// create compute fence and event
		m_computeFenceEvent.Attach(CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS));
		if (!m_computeFenceEvent.IsValid())
		{
			throw std::exception("CreateEvent");
		}

		DX_API("compute fence")
			device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_computeFence.ReleaseAndGetAddressOf()));
		m_computeFence->SetName(L"Compute");
		m_computeFenceValue = 1;

		DX_API("render resource fence")
			device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_renderResourceFence.ReleaseAndGetAddressOf()));
		m_renderResourceFence->SetName(L"Resource");
		m_renderResourceFenceValue = 1;

		D3D12_DESCRIPTOR_HEAP_DESC dhd;
		dhd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		dhd.NodeMask = 0;
		dhd.NumDescriptors = 2;
		dhd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

		DX_API("Failed to create descriptor heap for uavs")
			device->CreateDescriptorHeap(&dhd, IID_PPV_ARGS(uavHeap.GetAddressOf()));

		const D3D12_HEAP_PROPERTIES defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		const D3D12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(4*32*32*32, 
			D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 0);
		m_stateArray[0] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		DX_API("commited resource")
			device->CreateCommittedResource(
				&defaultHeapProperties,
				D3D12_HEAP_FLAG_NONE,
				&bufferDesc,
				m_stateArray[0],
				nullptr,
				IID_PPV_ARGS(m_array[0].ReleaseAndGetAddressOf()));
		m_array[0]->SetName(L"Unsorted input");

		m_stateArray[1] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		DX_API("commited resource")
			device->CreateCommittedResource(
				&defaultHeapProperties,
				D3D12_HEAP_FLAG_NONE,
				&bufferDesc,
				m_stateArray[1],
				nullptr,
				IID_PPV_ARGS(m_array[1].ReleaseAndGetAddressOf()));
		m_array[1]->SetName(L"Sorted output");

/*		const D3D12_RESOURCE_DESC bucketMatrixBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(4 * 16 * 32 * 32,
			D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 0);
		DX_API("commited resource")
			device->CreateCommittedResource(
				&defaultHeapProperties,
				D3D12_HEAP_FLAG_NONE,
				&bucketMatrixBufferDesc,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				nullptr,
				IID_PPV_ARGS(m_bucketMatrix[0].ReleaseAndGetAddressOf()));
		m_bucketMatrix[0]->SetName(L"Bucket size matrix.");*/

		CD3DX12_HEAP_PROPERTIES rbheapProps(D3D12_HEAP_TYPE_READBACK);

		D3D12_RESOURCE_ALLOCATION_INFO info = {};
		info.SizeInBytes = 4 * 32 * 32 * 32;
		info.Alignment = 0;
		const D3D12_RESOURCE_DESC tempBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(info);
		DX_API("problem")
			device->CreateCommittedResource(
				&rbheapProps,
				D3D12_HEAP_FLAG_NONE,
				&tempBufferDesc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(m_arrayReadback.ReleaseAndGetAddressOf()));


		CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);

		DX_API("problem")
			device->CreateCommittedResource(
				&heapProps,
				D3D12_HEAP_FLAG_NONE,
				&tempBufferDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(m_arrayData[0].ReleaseAndGetAddressOf()));

		void* pData;
		CD3DX12_RANGE range(0, info.SizeInBytes / 4);
		m_arrayData[0]->Map(0, &range, &pData);
		m_arrayDataBegin = reinterpret_cast<unsigned int*>( pData);
		m_arrayDataEnd = m_arrayDataBegin + info.SizeInBytes / 4;

		for (auto ip = m_arrayDataBegin; ip < m_arrayDataEnd; ip++) {
			*ip = rand() & 0xff;
			*ip |= (rand() & 0xff) << 8;
			*ip |= (rand() & 0xff) << 16;
			*ip |= (rand() & 0xff) << 24;
		}

		m_arrayDataBegin[ 0] = 0x1454abff;
		m_arrayDataBegin[ 1] = 0xa1667600;
		m_arrayDataBegin[ 2] = 0x23144bca;
		m_arrayDataBegin[ 3] = 0x004156fe;
		m_arrayDataBegin[ 4] = 0xf4541bff;
		m_arrayDataBegin[ 5] = 0xa5667100;
		m_arrayDataBegin[ 6] = 0x232441ca;
		m_arrayDataBegin[ 7] = 0x004156f1;
		m_arrayDataBegin[ 8] = 0x4454abfd;
		m_arrayDataBegin[ 9] = 0xa4667600;
		m_arrayDataBegin[10] = 0x23444bca;
		m_arrayDataBegin[11] = 0x004456fe;
		m_arrayDataBegin[12] = 0x34544bfb;
		m_arrayDataBegin[13] = 0xa4667400;
		m_arrayDataBegin[14] = 0x235444cb;
		m_arrayDataBegin[15] = 0x004656f4;
		m_arrayDataBegin[16] = 0xf4547bfc;
		m_arrayDataBegin[17] = 0xaf667800;
		m_arrayDataBegin[18] = 0x23f44b9a;
		m_arrayDataBegin[19] = 0x004f56fa;
		m_arrayDataBegin[20] = 0xf454fbff;
		m_arrayDataBegin[21] = 0xa5667f09;
		m_arrayDataBegin[22] = 0x3243fca;
		m_arrayDataBegin[23] = 0xc04156ff;
		m_arrayDataBegin[24] = 0xfc54abf8;
		m_arrayDataBegin[25] = 0xa5c67606;
		m_arrayDataBegin[26] = 0x232c4b57;
		m_arrayDataBegin[27] = 0x0041c4fe;
		m_arrayDataBegin[28] = 0xf4543cf5;
		m_arrayDataBegin[29] = 0xa56976c0;
		m_arrayDataBegin[30] = 0x23844bc3;
		m_arrayDataBegin[31] = 0x074156f2;

		m_arrayData[0]->Unmap(0, &range);

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
		uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.CounterOffsetInBytes = 0;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
		uavDesc.Buffer.NumElements = 32 * 32 * 32;
		uavDesc.Buffer.StructureByteStride = 0;
		// create uav
		device->CreateUnorderedAccessView(m_array[0].Get(), nullptr, &uavDesc, uavHeap->GetCPUDescriptorHandleForHeapStart());
		device->CreateUnorderedAccessView(m_array[1].Get(), nullptr, &uavDesc,
			CD3DX12_CPU_DESCRIPTOR_HANDLE(uavHeap->GetCPUDescriptorHandleForHeapStart(), 1, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)));

/*		D3D12_UNORDERED_ACCESS_VIEW_DESC bucketMatrixUavDesc;
		bucketMatrixUavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		bucketMatrixUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		bucketMatrixUavDesc.Buffer.CounterOffsetInBytes = 0;
		bucketMatrixUavDesc.Buffer.FirstElement = 0;
		bucketMatrixUavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
		bucketMatrixUavDesc.Buffer.NumElements = 16 * 32 * 32;
		bucketMatrixUavDesc.Buffer.StructureByteStride = 0;
		// create uav
		device->CreateUnorderedAccessView(m_bucketMatrix[0].Get(), nullptr, &bucketMatrixUavDesc,
			CD3DX12_CPU_DESCRIPTOR_HANDLE(uavHeap->GetCPUDescriptorHandleForHeapStart(), 1, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV))
			);
*/
		com_ptr<ID3DBlob> computeShader = Egg::Shader::LoadCso("Shaders/csLocalSort.cso");
		com_ptr<ID3D12RootSignature> rootSig = Egg::Shader::LoadRootSignature(device.Get(), computeShader.Get());
		m_computeRootSignature = rootSig;

		D3D12_COMPUTE_PIPELINE_STATE_DESC descComputePSO = {};
		descComputePSO.pRootSignature = rootSig.Get();
		descComputePSO.CS.pShaderBytecode = computeShader->GetBufferPointer();
		descComputePSO.CS.BytecodeLength = computeShader->GetBufferSize();

		DX_API("Failed to create compute pipeline state object.")
			device->CreateComputePipelineState(&descComputePSO, IID_PPV_ARGS(m_computePSO.ReleaseAndGetAddressOf()));
		m_computePSO->SetName(L"Compute PSO");

		com_ptr<ID3DBlob> mergeShader = Egg::Shader::LoadCso("Shaders/csMerge.cso");
		com_ptr<ID3D12RootSignature> mergeRootSig = Egg::Shader::LoadRootSignature(device.Get(), mergeShader.Get());
		m_mergeRootSignature = mergeRootSig;

		D3D12_COMPUTE_PIPELINE_STATE_DESC descMergePSO = {};
		descMergePSO.pRootSignature = mergeRootSig.Get();
		descMergePSO.CS.pShaderBytecode = mergeShader->GetBufferPointer();
		descMergePSO.CS.BytecodeLength = mergeShader->GetBufferSize();

		DX_API("Failed to create compute pipeline state object.")
			device->CreateComputePipelineState(&descMergePSO, IID_PPV_ARGS(m_mergePSO.ReleaseAndGetAddressOf()));
		m_mergePSO->SetName(L"Merge PSO");

		// Create compute allocator, command queue and command list
		D3D12_COMMAND_QUEUE_DESC descCommandQueue = { D3D12_COMMAND_LIST_TYPE_COMPUTE, 0, D3D12_COMMAND_QUEUE_FLAG_NONE };
		DX_API("Failed to create compute command queue.")
			device->CreateCommandQueue(&descCommandQueue, IID_PPV_ARGS(m_computeCommandQueue.ReleaseAndGetAddressOf()));

		DX_API("Failed to create compute command allocator.")
			device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(m_computeAllocator.ReleaseAndGetAddressOf()));

		DX_API("Failed to create compute command list.")
			device->CreateCommandList(
				0,
				D3D12_COMMAND_LIST_TYPE_COMPUTE,
				m_computeAllocator.Get(),
				m_computePSO.Get(),
				IID_PPV_ARGS(m_computeCommandList.ReleaseAndGetAddressOf()));

	}

	virtual void ReleaseResources() override {
		uavHeap.Reset();

		Egg::SimpleApp::ReleaseResources();
	}

	virtual void LoadAssets() override {

		DX_API("Failed to reset command allocator (UploadResources)")
			commandAllocator->Reset();
		DX_API("Failed to reset command list (UploadResources)")
			commandList->Reset(commandAllocator.Get(), nullptr);

		ResourceBarrier(commandList.Get(), m_array[0].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);
		ResourceBarrier(commandList.Get(), m_array[1].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);

		commandList->CopyResource(m_array[0].Get(), m_arrayData[0].Get());

		ResourceBarrier(commandList.Get(), m_array[0].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		ResourceBarrier(commandList.Get(), m_array[1].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		DX_API("Failed to close command list (UploadResources)")
			commandList->Close();

		m_computeResumeSignal.Attach(CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE));
		if (!m_computeResumeSignal.IsValid())
			throw std::exception("CreateEvent");

		m_usingAsyncCompute = true;

		m_computeThread = new std::thread(&ggl004App::AsyncComputeThreadProc, this);

		ID3D12CommandList* commandLists[] = { commandList.Get() };
		commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

		WaitForPreviousFrame();

		//release uploadresources
	}

	std::atomic<bool>                           m_terminateThread;
	std::atomic<bool>                           m_suspendThread;
	std::thread* m_computeThread;
	std::atomic<bool>                           m_usingAsyncCompute;

	void AsyncComputeThreadProc()
	{
		while (!m_terminateThread)
		{
			if (m_suspendThread)
			{
				(void)WaitForSingleObject(m_computeResumeSignal.Get(), INFINITE);
			}

			if (m_usingAsyncCompute)
			{
				//if (m_windowUpdated)
				{
//					while (true)
//					{
//						if (m_resourceState[0/*ComputeIndex()*/] == ResourceState_Switching)						// render kicked off a resource switch to unordered,
//						{																					// check the fence for completed for quickest turn around
//							if (m_renderResourceFence->GetCompletedValue() >= m_renderResourceFenceValue)	// render might also check first and switch the state to ready compute
//							{
//								m_resourceState[0/*ComputeIndex()*/] = ResourceState_ReadyCompute;
//								break;
//							}
//						}
//						if (m_resourceState[0/*ComputeIndex()*/] == ResourceState_ReadyCompute)					// render detected compute buffer switched to unordered access first
//						{
//							break;
//						}
//						if (!m_usingAsyncCompute)															// user has request synchronous compute
//						{
//							break;
//						}
//					}
//					if (!m_usingAsyncCompute)																// user has request synchronous compute
//					{
//						continue;
//					}

					if (m_suspendThread)
					{
						(void)WaitForSingleObject(m_computeResumeSignal.Get(), INFINITE);
					}

					//UpdateFractalData();

					ID3D12DescriptorHeap* pHeaps[] = { uavHeap.Get() };
					m_computeCommandList->SetDescriptorHeaps(_countof(pHeaps), pHeaps);

					m_resourceState[0] = ResourceState_Computing;

					m_computeCommandList->SetComputeRootSignature(m_computeRootSignature.Get());

					m_computeCommandList->SetComputeRootDescriptorTable(1, uavHeap->GetGPUDescriptorHandleForHeapStart());

					D3D12_RESOURCE_BARRIER uavBarrier;
					uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
					uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
					uavBarrier.UAV.pResource = m_array[0].Get();

					D3D12_RESOURCE_BARRIER uav1Barrier;
					uav1Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
					uav1Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
					uav1Barrier.UAV.pResource = m_array[1].Get();

					m_computeCommandList->SetPipelineState(m_computePSO.Get());
					m_computeCommandList->SetComputeRoot32BitConstant(0, 0, 0);
					m_computeCommandList->Dispatch(32, 1, 1);
					m_computeCommandList->ResourceBarrier(1, &uavBarrier);
					m_computeCommandList->SetComputeRoot32BitConstant(0, 4, 0);
					m_computeCommandList->Dispatch(32, 1, 1);
					m_computeCommandList->ResourceBarrier(1, &uavBarrier);
					m_computeCommandList->SetComputeRoot32BitConstant(0, 8, 0);
					m_computeCommandList->Dispatch(32, 1, 1);
					m_computeCommandList->ResourceBarrier(1, &uavBarrier);
					m_computeCommandList->SetComputeRoot32BitConstant(0, 12, 0);
					m_computeCommandList->Dispatch(32, 1, 1);
					m_computeCommandList->ResourceBarrier(1, &uavBarrier);
					m_computeCommandList->SetComputeRoot32BitConstant(0, 16, 0);
					m_computeCommandList->Dispatch(32, 1, 1);
					m_computeCommandList->ResourceBarrier(1, &uavBarrier);
					m_computeCommandList->SetComputeRoot32BitConstant(0, 20, 0);
					m_computeCommandList->Dispatch(32, 1, 1);
					m_computeCommandList->ResourceBarrier(1, &uavBarrier);
					m_computeCommandList->SetComputeRoot32BitConstant(0, 24, 0);
					m_computeCommandList->Dispatch(32, 1, 1);
					m_computeCommandList->ResourceBarrier(1, &uavBarrier);
					m_computeCommandList->SetComputeRoot32BitConstant(0, 28, 0);
					m_computeCommandList->Dispatch(32, 1, 1);
					m_computeCommandList->ResourceBarrier(1, &uavBarrier);

					m_computeCommandList->SetComputeRootSignature(m_mergeRootSignature.Get());
					m_computeCommandList->SetComputeRootDescriptorTable(1, uavHeap->GetGPUDescriptorHandleForHeapStart());

					m_computeCommandList->SetPipelineState(m_mergePSO.Get());
					m_computeCommandList->Dispatch(1, 1, 1);
					m_computeCommandList->ResourceBarrier(1, &uav1Barrier);

					// close and execute the command list
					m_computeCommandList->Close();
					ID3D12CommandList* tempList = m_computeCommandList.Get();
					m_computeCommandQueue->ExecuteCommandLists(1, &tempList);

					const uint64_t fence = m_computeFenceValue++;
					m_computeCommandQueue->Signal(m_computeFence.Get(), fence);
					if (m_computeFence->GetCompletedValue() < fence)								// block until async compute has completed using a fence
					{
						m_computeFence->SetEventOnCompletion(fence, m_computeFenceEvent.Get());
						WaitForSingleObject(m_computeFenceEvent.Get(), INFINITE);
					}
					m_resourceState[0/*ComputeIndex()*/] = ResourceState_Computed;						// signal the buffer is now ready for render thread to use

					m_computeAllocator->Reset();
					m_computeCommandList->Reset(m_computeAllocator.Get(), m_computePSO.Get());

					break;
				}
				//else
				//{
				//	SwitchToThread();
				//}
			}
			else
			{
				SwitchToThread();
			}
		}
	}
};


