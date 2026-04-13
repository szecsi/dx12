#pragma once
#include "Egg/Common.h"
#include <Egg/Script/ScriptedApp.h>
#include <d3d11on12.h>
#include <algorithm>

#include "Float4Buffer.h"
#include "RawBuffer.h"
#include "ComputePass.h"
#include "WaveSort.h"
#include "FenceChain.h"
#include "Particle.h"

bool verifyStarterCount(uint* data, uint* starterCount) {
	uint prev = 0xffffffff;
	for (uint iPage = 0; iPage < 32; iPage++)
	{
		uint aStarters = 0;
		uint aLeadingNonStarters = 0;
		if (data[iPage * 32 * 32]  == 0xffffffff)
			return true;
		for (uint i = 0; i < 32 * 32; i++) {
			uint c = data[iPage * 32 * 32 + i];

			if (c != prev)
				aStarters++;
			if (aStarters == 0)
				aLeadingNonStarters++;
			prev = c;
		}
		if (starterCount[iPage] != ((aLeadingNonStarters << 16) | aStarters))
				return false;
	}
	return true;
}

struct MaskedComp {
	MaskedComp(uint offsets) : maskOffsets(offsets){}
	uint maskOffsets;
	bool operator()(const uint& a, const uint& b)const {
		uint ma =
			(a >> (maskOffsets & 0xff)) & 0x1 |
			(a >> ((maskOffsets & 0xff00) >> 8) << 1) & 0x2 |
			(a >> ((maskOffsets & 0xff0000) >> 16) << 2) & 0x4 |
			(a >> ((maskOffsets & 0xff000000) >> 24) << 3) & 0x8;
		uint mb =
			(b >> (maskOffsets & 0xff)) & 0x1 |
			(b >> ((maskOffsets & 0xff00) >> 8) << 1) & 0x2 |
			(b >> ((maskOffsets & 0xff0000) >> 16) << 2) & 0x4 |
			(b >> ((maskOffsets & 0xff000000) >> 24) << 3) & 0x8;
		return ma < mb;
	}
};

struct MortonComp {
	uint mortonHashFromCellIndex(uint a) const {
		uint x = (a      ) & 0x7ff;
		uint y = (a >> 11) & 0x7ff;
		uint z = (a >> 22) & 0x7ff;
		uint hash = 0;
		uint i;
		for (i = 0; i < 7; ++i)
		{
			hash |= ((x & (1 << i)) << (2 * i)) | ((y & (1 << i)) << (2 * i + 1)) | ((z & (1 << i)) << (2 * i + 2));
		}
		return hash;
	}

	bool operator()(const uint& a, const uint& b)const {
		return mortonHashFromCellIndex(a) < mortonHashFromCellIndex(b);
	}
};

struct KeyComp {
	uint* pKeys;
	uint mask;
	KeyComp(uint* pKeys, uint mask = 0xffffffff) :pKeys(pKeys),mask(mask) {}
	bool operator()(const uint& a, const uint& b)const {
		return (pKeys[a>>12] & mask) < (pKeys[b>>12] & mask);
	}
};

class RetamApp : public Egg::Script::ScriptedApp {

	uint frameCount;
protected:

	com_ptr<ID3D12CommandQueue>  computeCommandQueue;
	std::vector < com_ptr<ID3D12CommandAllocator> > computeAllocators;
	std::vector < com_ptr<ID3D12GraphicsCommandList> > computeCommandLists;

	com_ptr<ID3D12CommandAllocator> uploadAllocator;
	com_ptr<ID3D12GraphicsCommandList> uploadCommandList;

	Fence uploadFence;
	FenceChain computeFenceChain;
	FenceChain graphicsFenceChain;

	com_ptr<ID3D12DescriptorHeap> uavHeap;
	std::vector<RawBuffer> buffers;

#define BUFFERNAMES 		keys, perPageBucketOffsets, indicesWithKeyBits0, indicesWithKeyBits1, globalBucketOffsets

	enum BufferRole {
		BUFFERNAMES
	};
	static std::wstring bufferToString(BufferRole r) {
		switch (r) {
		case keys: return L"keys";
		case perPageBucketOffsets: return L"perPageBucketOffsets";
		case indicesWithKeyBits0: return L"indicesWithKeyBits0";
		case indicesWithKeyBits1: return L"indicesWithKeyBits1";
		case globalBucketOffsets: return L"globalBucketOffsets";
		}
	}

	WaveSort waveSort;

public:
	RetamApp() : ScriptedApp() {}

	virtual void CreateResources() override {
		Egg::Script::ScriptedApp::CreateResources();

		frameCount = 0;

		uploadFence.createResources(device);

		D3D12_DESCRIPTOR_HEAP_DESC dhd;
		dhd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		dhd.NodeMask = 0;
		dhd.NumDescriptors = 7;
		dhd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		uint dhIncrSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		DX_API("create descriptor heap for uavs")
			device->CreateDescriptorHeap(&dhd, IID_PPV_ARGS(uavHeap.GetAddressOf()));

		for (auto name : { BUFFERNAMES }) {
			std::wstring wname = bufferToString(name);
			buffers.push_back(RawBuffer(wname));
			CD3DX12_CPU_DESCRIPTOR_HANDLE handle(
				uavHeap->GetCPUDescriptorHandleForHeapStart(),
				name,
				device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
			buffers[name].createResources(device, handle);
		}

		buffers[keys].fillRandomMask(0xffffffff);

		auto dhStart = CD3DX12_GPU_DESCRIPTOR_HANDLE(uavHeap->GetGPUDescriptorHandleForHeapStart());
		ComputeShader csLocalSortAlpha;
		ComputeShader csLocalSortBeta;
		ComputeShader csLocalSortGamma;
		ComputeShader csScan;
		ComputeShader csPackAlpha;
		ComputeShader csPackBeta;
		ComputeShader csPackGamma;
		csLocalSortAlpha.createResources(device, "Shaders/RadixSort/csLocalSortAlpha.cso");
		csLocalSortBeta.createResources(device, "Shaders/RadixSort/csLocalSortBeta.cso");
		csLocalSortGamma.createResources(device, "Shaders/RadixSort/csLocalSortGamma.cso");
		csScan.createResources(device, "Shaders/RadixSort/csScan.cso");
		csPackAlpha.createResources(device, "Shaders/RadixSort/csPackAlpha.cso");
		csPackBeta.createResources(device, "Shaders/RadixSort/csPackBeta.cso");
		csPackGamma.createResources(device, "Shaders/RadixSort/csPackGamma.cso");

		waveSort.creaseResources(
			csLocalSortAlpha,
			csLocalSortBeta,
			csLocalSortGamma,
			csScan,
			csPackAlpha,
			csPackBeta,
			csPackGamma,
			dhStart, 0, dhIncrSize, buffers, false);

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

		buffers[keys].upload(uploadCommandList);

		DX_API("close command list.")
			uploadCommandList->Close();
		ID3D12CommandList* ppCommandLists[] = { uploadCommandList.Get() };
		commandQueue->ExecuteCommandLists(1, ppCommandLists);

		uploadFence.signal(commandQueue, 1);
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
	}

	void SceneUploadResources() {
		DX_API("Failed to reset command allocator (UploadResources)")
			commandAllocator->Reset();
		DX_API("Failed to reset command list (UploadResources)")
			commandList->Reset(commandAllocator.Get(), nullptr);

		__super::UploadResources();

		DX_API("Failed to close command list (UploadResources)")
			commandList->Close();

		ID3D12CommandList* commandLists[] = { commandList.Get() };
		commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

		WaitForPreviousFrame();

		__super::ReleaseUploadResources();
	}

	virtual void LoadAssets() override {
		using namespace Egg::Scene;
		using namespace Egg::Mesh;

		__super::LoadAssets();

		RunScript("scene.lua");

		SceneUploadResources();
	}

	void recordComputeCommands() {
		ID3D12DescriptorHeap* pHeaps[] = { uavHeap.Get() };
		computeCommandLists[swapChainBackBufferIndex]->SetDescriptorHeaps(_countof(pHeaps), pHeaps);

		waveSort.populate(computeCommandLists[swapChainBackBufferIndex]);
	}

	virtual void PopulateCommandList() override {

		// --- Compute pass ---
		computeAllocators[swapChainBackBufferIndex]->Reset();
		auto& computeCommandList = computeCommandLists[swapChainBackBufferIndex];
		computeCommandList->Reset(computeAllocators[swapChainBackBufferIndex].Get(), nullptr);

		recordComputeCommands();

		DX_API("close compute command list")
			computeCommandLists[swapChainBackBufferIndex]->Close();
		{
			ID3D12CommandList* ppCommandLists[] = { computeCommandLists[swapChainBackBufferIndex].Get() };
			computeCommandQueue->ExecuteCommandLists(1, ppCommandLists);
		}
		computeFenceChain.signal(computeCommandQueue, swapChainBackBufferIndex);

		// Wait for compute before graphics
		computeFenceChain.gpuWait(commandQueue, swapChainBackBufferIndex);

		// --- Graphics/render pass ---
		commandAllocator->Reset();
		commandList->Reset(commandAllocator.Get(), nullptr);

		commandList->RSSetViewports(1, &viewPort);
		commandList->RSSetScissorRects(1, &scissorRect);

		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[swapChainBackBufferIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

		CD3DX12_CPU_DESCRIPTOR_HANDLE rHandle(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), swapChainBackBufferIndex, rtvDescriptorHandleIncrementSize);
		CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(dsvHeap->GetCPUDescriptorHandleForHeapStart());
		commandList->OMSetRenderTargets(1, &rHandle, FALSE, &dsvHandle);

		const float clearColor[] = { 0.5f, 0.2f, 0.4f, 1.0f };
		commandList->ClearRenderTargetView(rHandle, clearColor, 0, nullptr);
		commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		__super::PopulateCommandList();

		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[swapChainBackBufferIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

		DX_API("close graphics command list")
			commandList->Close();

		ID3D12CommandList* cLists[] = { commandList.Get() };
		commandQueue->ExecuteCommandLists(_countof(cLists), cLists);

		graphicsFenceChain.signal(commandQueue, swapChainBackBufferIndex);

		WaitForPreviousFrame();

		frameCount++;
	}

	virtual void Resize(int width, int height) override {
		Egg::Script::ScriptedApp::Resize(width, height);
	}

	virtual void Update(float dt, float T) override {
		__super::Update(dt, T);
	}

	virtual void ProcessMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override {
		__super::ProcessMessage(hWnd, uMsg, wParam, lParam);
	}

	virtual void ReleaseSwapChainResources() override {
		__super::ReleaseSwapChainResources();
	}

	virtual void ReleaseResources() override {
		uavHeap.Reset();
		Egg::Script::ScriptedApp::ReleaseResources();
	}
};
