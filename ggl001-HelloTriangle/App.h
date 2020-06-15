#pragma once

#include "Common.h"

// R"()" is the multiline string in C++
const char * vertexShaderCode = R"(
//Input Assembler Output Structure
struct IAOS {
	float3 position : POSITION;
};

//Vertex Shader Output Structure
struct VSOS {
	float4 position : SV_Position;
};

VSOS vs_main(IAOS iaos) {
	VSOS vsos;
	vsos.position = float4(iaos.position.xy, 0.0f, 1.0f);
	return vsos;
}
)";

const char * pixelShaderCode = R"(
//Vertex Shader Output Structure
struct VSOS {
	float4 position : SV_Position;
};

float4 ps_main(VSOS vsos) : SV_Target {
	return float4(0.1f, 0.2f, 0.8f, 1.0f);
}
)";

class App {
protected:
	constexpr static unsigned int BACKBUFFER_DEPTH = 2;

	D3D12_VIEWPORT viewPort;
	D3D12_RECT scissorRect;

	com_ptr<IDXGISwapChain3> swapChain;
	com_ptr<ID3D12Device> device;
	com_ptr<ID3D12CommandQueue> commandQueue;
	com_ptr<ID3D12DescriptorHeap> rtvDescriptorHeap;
	unsigned int rtvDescriptorHandleIncrementSize;
	com_ptr<ID3D12Resource> renderTargets[BACKBUFFER_DEPTH];

	com_ptr<ID3D12CommandAllocator> commandAllocator;
	com_ptr<ID3D12GraphicsCommandList> commandList;

	// Graphics Pipeline State Object
	com_ptr<ID3DBlob> vsByteCode;
	com_ptr<ID3DBlob> psByteCode;
	com_ptr<ID3D12RootSignature> rootSignature;
	com_ptr<ID3D12PipelineState> gpso;

	// Sync objects
	com_ptr<ID3D12Fence> fence;
	HANDLE fenceEvent;
	unsigned int frameIndex;
	unsigned long long fenceValue;

	// Asset Data
	com_ptr<ID3D12Resource> vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;


	void PopulateCommandList() {
		commandAllocator->Reset();
		commandList->Reset(commandAllocator.Get(), gpso.Get());


		commandList->SetGraphicsRootSignature(rootSignature.Get());
		commandList->RSSetViewports(1, &viewPort);
		commandList->RSSetScissorRects(1, &scissorRect);

		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

		CD3DX12_CPU_DESCRIPTOR_HANDLE rHandle(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), frameIndex, rtvDescriptorHandleIncrementSize);
		commandList->OMSetRenderTargets(1, &rHandle, FALSE, nullptr);

		const float clearColor[] = { 0.0f, 0.1f, 0.2f, 1.0f };
		commandList->ClearRenderTargetView(rHandle, clearColor, 0, nullptr);

		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		commandList->DrawInstanced(3, 1, 0, 0);

		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

		DX_API("Failed to close command list")
			commandList->Close();
	}

	void WaitForPreviousFrame() {
		const UINT64 fv = fenceValue;
		DX_API("Failed to signal from command queue")
			commandQueue->Signal(fence.Get(), fv);

		fenceValue++;

		if(fence->GetCompletedValue() < fv) {
			DX_API("Failed to sign up for event completion")
				fence->SetEventOnCompletion(fv, fenceEvent);
			WaitForSingleObject(fenceEvent, INFINITE);
		}

		frameIndex = swapChain->GetCurrentBackBufferIndex();
	}


public:
	virtual void Render() {
		PopulateCommandList();

		// Execute
		ID3D12CommandList * cLists[] = { commandList.Get() };
		commandQueue->ExecuteCommandLists(_countof(cLists), cLists);

		DX_API("Failed to present swap chain")
			swapChain->Present(0, 0);

		// Sync
		WaitForPreviousFrame();
	}

	virtual void LoadAssets() {
		struct Vertex {
			float pos[3];
		};

		Vertex triangleVertices[] = {
			{ {  0.0f   ,  0.85f  , 0.0f } },
			{ {  0.7071f, -0.8571f, 0.0f } },
			{ { -0.7071f, -0.8571f, 0.0f } }
		};

		unsigned int vertexBufferSize = sizeof(triangleVertices);

		DX_API("Failed to create commited resource")
			device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
											D3D12_HEAP_FLAG_NONE,
											&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
											D3D12_RESOURCE_STATE_GENERIC_READ,
											nullptr,
											IID_PPV_ARGS(vertexBuffer.GetAddressOf()));

		vertexBuffer->SetName(L"Vertex Buffer");

		UINT8 * vertexDataBegin;
		CD3DX12_RANGE readRange(0, 0); // CPU read is not mapped
		vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&vertexDataBegin));
		memcpy(vertexDataBegin, triangleVertices, vertexBufferSize);
		vertexBuffer->Unmap(0, nullptr);

		vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
		vertexBufferView.StrideInBytes = sizeof(Vertex);
		vertexBufferView.SizeInBytes = vertexBufferSize;
	}

	virtual void CreateResources() {

		// Create Root Signature

		D3D12_ROOT_SIGNATURE_DESC rootDesc = { 0 };
		rootDesc.NumParameters = 0;
		rootDesc.NumStaticSamplers = 0;
		rootDesc.pStaticSamplers = nullptr;
		rootDesc.pParameters = nullptr;
		rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		com_ptr<ID3DBlob> signature;
		com_ptr<ID3DBlob> error;

		DX_API("Failed to serialize root signature")
			D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, signature.GetAddressOf(), error.GetAddressOf());

		DX_API("Failed to create root signature")
			device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(rootSignature.GetAddressOf()));

		// Create Command Allocator




		// Input Layout

		D3D12_INPUT_ELEMENT_DESC inputDesc[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		D3D12_INPUT_LAYOUT_DESC inputLayout;
		inputLayout.NumElements = _countof(inputDesc);
		inputLayout.pInputElementDescs = inputDesc;

		// Vertex / Pixel Shaders

		com_ptr<ID3DBlob> vsError;
		com_ptr<ID3DBlob> psError;

		HRESULT cRes = D3DCompile(vertexShaderCode, strlen(vertexShaderCode), "basic vertex shader", nullptr, nullptr, "vs_main", "vs_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, vsByteCode.GetAddressOf(), vsError.GetAddressOf());

		ASSERT(SUCCEEDED(cRes), BlobAsString(vsError));

		cRes = D3DCompile(pixelShaderCode, strlen(pixelShaderCode), "basic pixel shader", nullptr, nullptr, "ps_main", "ps_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, psByteCode.GetAddressOf(), psError.GetAddressOf());

		ASSERT(SUCCEEDED(cRes), BlobAsString(psError));

		// Create GPSO

		D3D12_GRAPHICS_PIPELINE_STATE_DESC gpsoDesc;
		ZeroMemory(&gpsoDesc, sizeof(gpsoDesc));
		gpsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		gpsoDesc.NumRenderTargets = 1;
		gpsoDesc.DepthStencilState.StencilEnable = FALSE;
		gpsoDesc.DepthStencilState.DepthEnable = FALSE;
		gpsoDesc.SampleMask = UINT_MAX;
		gpsoDesc.SampleDesc.Count = 1;
		gpsoDesc.SampleDesc.Quality = 0;
		gpsoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		gpsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		gpsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		gpsoDesc.pRootSignature = rootSignature.Get();
		gpsoDesc.InputLayout = inputLayout;
		gpsoDesc.VS = CD3DX12_SHADER_BYTECODE(vsByteCode.Get());
		gpsoDesc.PS = CD3DX12_SHADER_BYTECODE(psByteCode.Get());

		DX_API("Failed to initialize GPSO")
			device->CreateGraphicsPipelineState(&gpsoDesc, IID_PPV_ARGS(gpso.GetAddressOf()));

		DX_API("Failed to create command allocator")
			device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(commandAllocator.GetAddressOf()));

		// Create Graphics Command List, set the newly created GPSO as its initial state

		DX_API("Failed to greate graphics command list")
			device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), gpso.Get(), IID_PPV_ARGS(commandList.GetAddressOf()));

		DX_API("Failed to close command list")
			commandList->Close();

		// Create Sync Objects, fenceEvent is WinAPI, not DirectX

		DX_API("Failed to create fence")
			device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.GetAddressOf()));
		fenceValue = 1;

		fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		if(fenceEvent == NULL) {
			DX_API("Failed to create windows event") HRESULT_FROM_WIN32(GetLastError());
		}

		WaitForPreviousFrame();
	}

	virtual void ReleaseAssets() {
		vertexBuffer.Reset();
	}

	virtual void ReleaseResources() {
		commandList.Reset();
		vsByteCode.Reset();
		psByteCode.Reset();
		rootSignature.Reset();
		gpso.Reset();
		fence.Reset();
		commandAllocator.Reset();
	}

	virtual void CreateSwapChainResources() {
		DXGI_SWAP_CHAIN_DESC scDesc;
		swapChain->GetDesc(&scDesc);

		viewPort.TopLeftX = 0;
		viewPort.TopLeftY = 0;
		viewPort.Width = scDesc.BufferDesc.Width;
		viewPort.Height = scDesc.BufferDesc.Height;

		scissorRect.left = 0;
		scissorRect.top = 0;
		scissorRect.right = scDesc.BufferDesc.Width;
		scissorRect.bottom = scDesc.BufferDesc.Height;

		// Create Render Target View Descriptor Heap, like a RenderTargetView** on the GPU. A set of pointers.

		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.NumDescriptors = BACKBUFFER_DEPTH;
		rtvHeapDesc.NodeMask = 0;

		DX_API("Failed to create render target view descriptor heap")
			device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(rtvDescriptorHeap.GetAddressOf()));


		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		rtvDescriptorHandleIncrementSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		// Create Render Target Views

		for(unsigned int i = 0; i < BACKBUFFER_DEPTH; ++i) {
			DX_API("Failed to get swap chain buffer")
				swapChain->GetBuffer(i, IID_PPV_ARGS(renderTargets[i].GetAddressOf()));

			CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle{ rtvHandle };
			cpuHandle.Offset(i * rtvDescriptorHandleIncrementSize);

			device->CreateRenderTargetView(renderTargets[i].Get(), nullptr, cpuHandle);
		}

		frameIndex = swapChain->GetCurrentBackBufferIndex();
	}

	void Resize(int w, int h) {
		WaitForPreviousFrame();

		ReleaseSwapChainResources();
		DX_API("Failed to resize swap chain")
			swapChain->ResizeBuffers(BACKBUFFER_DEPTH, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
		CreateSwapChainResources();
	}

	virtual void ReleaseSwapChainResources() {
		for(unsigned int i = 0; i < BACKBUFFER_DEPTH; ++i) {
			renderTargets[i].Reset();
		}
		rtvDescriptorHeap.Reset();
	}

	virtual void Destroy() {
		WaitForPreviousFrame();
		ReleaseSwapChainResources();
		ReleaseResources();
		ReleaseAssets();
		commandQueue.Reset();
		swapChain.Reset();
		device.Reset();
	}

	virtual void SetCommandQueue(com_ptr<ID3D12CommandQueue> cQueue) {
		commandQueue = cQueue;
	}

	virtual void SetDevice(com_ptr<ID3D12Device> dev) {
		device = dev;
	}

	virtual void SetSwapChain(com_ptr<IDXGISwapChain3> sChain) {
		swapChain = sChain;
	}
};
