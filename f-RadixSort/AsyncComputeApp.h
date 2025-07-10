#pragma once
#include "Egg/Common.h"
#include <Egg/SimpleApp.h>
#include <d3d11on12.h>
#include <algorithm>

#include "Float4Buffer.h"
#include "RawBuffer.h"
#include "ComputePass.h"
#include "WaveSort.h"
#include "Game.h"
#include "FenceChain.h"

bool verifyStarterCount(uint* data, uint* starterCount) {
	uint prev = 0xffffffff;
	for (uint iPage = 0; iPage < 32; iPage++)
	{
		uint aStarters = 0;
		uint aLeadingNonStarters = 0;
		if (data[iPage * 32 * 32]  == 0xffffffff)
			return true; // end of useful data, this is an empty page already
		for (uint i = 0; i < 32 * 32; i++) {
			uint c = data[iPage * 32 * 32 + i];

			if (c != prev)
				aStarters++;
			if (aStarters == 0)
				aLeadingNonStarters++;
			prev = c;
		}
		if (starterCount[iPage] != ((aLeadingNonStarters << 16) | aStarters))
//			if(aLeadingNonStarters != 32*32)
				return false;
	}
	return true;
}

struct MaskedComp {
	MaskedComp(uint offsets) : maskOffsets(offsets){
	}
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

class AsyncComputeApp : public Egg::SimpleApp {
	Egg11::App::P app11;

	uint frameCount;
protected:
	com_ptr<ID3D11On12Device> device11on12;
	com_ptr<ID3D11Device> device11;
	com_ptr<ID3D11Device2> device211;
	com_ptr<ID3D11DeviceContext> context11;
	std::vector<com_ptr<ID3D11Resource>> renderTargets11;
	std::vector<com_ptr<ID3D11RenderTargetView>> defaultRtvs11;
	std::vector < com_ptr<ID3D11Resource> > depthStencils11;
	std::vector < com_ptr<ID3D11DepthStencilView> > defaultDsvs11;

	com_ptr<ID3D12CommandQueue>  computeCommandQueue;
	std::vector < com_ptr<ID3D12CommandAllocator> > computeAllocators;
	std::vector < com_ptr<ID3D12GraphicsCommandList> > computeCommandLists;

	std::vector < com_ptr<ID3D12CommandAllocator> > copyAllocators;
	std::vector < com_ptr<ID3D12GraphicsCommandList> > copyCommandLists;

	com_ptr<ID3D12CommandAllocator> uploadAllocator;
	com_ptr<ID3D12GraphicsCommandList> uploadCommandList;

	Fence uploadFence;
	FenceChain computeFenceChain;
	FenceChain graphicsFenceChain;
	FenceChain copyFenceChain;

	com_ptr<ID3D12DescriptorHeap> uavHeap;
	std::vector<RawBuffer> buffers;

#define BUFFERNAMES 		mortons, pins, mortonPerPageBucketCounts, sortedPins, sortedMortons, sortedMortonPerPageStarterCounts, hashList, cellLut, hashPerPageBucketCounts, sortedCellLut, sortedHashList, sortedHashPerPageStarterCounts, hashLut

	enum BufferRole {
		BUFFERNAMES
	};
	static std::wstring bufferToString(BufferRole r) {
		switch (r) {
		case mortons: return L"mortons";
		case pins: return L"pins";
		case mortonPerPageBucketCounts: return L"mortonPerPageBucketCounts";
		case sortedPins: return L"sortedPins";
		case sortedMortons: return L"sortedMortons";
		case sortedMortonPerPageStarterCounts: return L"sortedMortonPerPageStarterCounts";
		case hashList: return L"hashList";
		case cellLut: return L"cellLut";
		case hashPerPageBucketCounts: return L"hashPerPageBucketCounts";
		case sortedCellLut: return L"sortedCellLut";
		case sortedHashList: return L"sortedHashList";
		case sortedHashPerPageStarterCounts: return L"sortedHashPerPageStarterCounts";
		case hashLut: return L"hashLut";
		}
	}

	WaveSort radixSort;
public:
	AsyncComputeApp() :SimpleApp(){}

	virtual void CreateResources() override {
		Egg::SimpleApp::CreateResources();

		frameCount = 0;

/*noelf		UINT d3d11DeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
		d3d11DeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;

		auto featl = D3D_FEATURE_LEVEL_11_0;
		DX_API("create d3d11 device")
			D3D11On12CreateDevice(
				device.Get(),
				d3d11DeviceFlags,
				nullptr,
				0,
				reinterpret_cast<IUnknown**>(commandQueue.GetAddressOf()),
				1,
				0,
				device11.GetAddressOf(),
				context11.GetAddressOf(),
				nullptr
			);

		device11->QueryInterface<ID3D11On12Device>(device11on12.GetAddressOf());
		device11->QueryInterface<ID3D11Device2>(device211.GetAddressOf());
		app11 = Game::Create(device211);*/

		uploadFence.createResources(device);

		D3D12_DESCRIPTOR_HEAP_DESC dhd;
		dhd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		dhd.NodeMask = 0;
		dhd.NumDescriptors = (uint)hashLut + 1; //TODO number of buffers
		dhd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		uint dhIncrSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		DX_API("create descriptor heap for uavs")
			device->CreateDescriptorHeap(&dhd, IID_PPV_ARGS(uavHeap.GetAddressOf()));

		for (auto name : { BUFFERNAMES }) {
			bool sharedWithD3D11 = false; //noelf name == mortons || name == sortedPins || name == sortedCellLut || name == hashLut;
			std::wstring wname = bufferToString(name);
			buffers.push_back(RawBuffer(wname, sharedWithD3D11));
			CD3DX12_CPU_DESCRIPTOR_HANDLE handle(
				uavHeap->GetCPUDescriptorHandleForHeapStart(),
				name,
				device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
			buffers[name].createResources(device, device11on12, handle);
			//noelf app11->setSharedResource(wname, buffers[name].getWrappedBuffer());
		}

		//noelf app11->createResources();

		buffers[mortons].fillRandom();
		//buffers[mortons].fillRandomMask(0x7);
		//buffers[mortons].fillFFFFFFFF();

		uint zeros[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
		uint ffffs[] = { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff };

		auto dhStart = CD3DX12_GPU_DESCRIPTOR_HANDLE(uavHeap->GetGPUDescriptorHandleForHeapStart());
		ComputeShader csLocalSortInPlace;
		ComputeShader csLocalSort;
		ComputeShader csMerge;
		csLocalSortInPlace.createResources(device, "Shaders/csLocalSortInPlace.cso");
		csLocalSort.createResources(device, "Shaders/csLocalSort.cso");
		csMerge.createResources(device, "Shaders/csPack.cso");

		mortonSort.creaseResources(csLocalSortInPlace, csLocalSort, csMerge, dhStart, mortons, dhIncrSize, buffers, false /*true*/);
		hashSort.creaseResources(csLocalSortInPlace, csLocalSort, csMerge, dhStart, hashList, dhIncrSize, buffers);

		ComputeShader csStarterCount;
		csStarterCount.createResources(device, "Shaders/csStarterCount.cso");
		mortonCountStarters.createResources(csStarterCount, dhStart, sortedMortons, dhIncrSize, buffers, 2);
		hashCountStarters.createResources(csStarterCount, dhStart, sortedHashList, dhIncrSize, buffers, 2);

		ComputeShader csCreateCellList;
		csCreateCellList.createResources(device, "Shaders/csCreateCellList.cso");
		createCellList.createResources(csCreateCellList, dhStart, sortedMortons, dhIncrSize, buffers, 4);

		ComputeShader csClearBuffer;
		csClearBuffer.createResources(device, "Shaders/csClearBuffer.cso");
		clearHashList.createResources(csClearBuffer, dhStart, hashList, dhIncrSize, buffers, 1, ffffs, 1);
		clearHashLut.createResources(csClearBuffer, dhStart, hashLut, dhIncrSize, buffers, 1, zeros, 1);

		ComputeShader csFillPins;
		csFillPins.createResources(device, "Shaders/csFillBufferIndices.cso");
		fillPins.createResources(csFillPins, dhStart, pins, dhIncrSize, buffers, 1);
		fillSortedPins.createResources(csFillPins, dhStart, sortedPins, dhIncrSize, buffers, 1);

		ComputeShader csCreateHashList;
		csCreateHashList.createResources(device, "Shaders/csCreateHashList.cso");
		createHashList.createResources(csCreateHashList, dhStart, sortedHashList, dhIncrSize, buffers, 3);

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

		buffers[mortons].upload(uploadCommandList);

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
		copyFenceChain.createResources(device, swapChainBackBufferCount);

		/* //noelf
		renderTargets11.resize(swapChainBackBufferCount);
		defaultRtvs11.resize(swapChainBackBufferCount);
		depthStencils11.resize(swapChainBackBufferCount);
		defaultDsvs11.resize(swapChainBackBufferCount);

		D3D11_RESOURCE_FLAGS d3d11Flags = { D3D11_BIND_RENDER_TARGET };
		D3D11_RESOURCE_FLAGS d3d11DepthFlags = { D3D11_BIND_DEPTH_STENCIL };
		for (uint i = 0; i < swapChainBackBufferCount; i++) {
			DX_API("wrap 12 back buffer for d3d11")
				device11on12->CreateWrappedResource(
					renderTargets[i].Get(),
					&d3d11Flags,
					D3D12_RESOURCE_STATE_RENDER_TARGET,
					D3D12_RESOURCE_STATE_PRESENT,
					IID_PPV_ARGS(renderTargets11[i].GetAddressOf())
				);

			com_ptr<ID3D11Texture2D> bbuftex;
			renderTargets11[i]->QueryInterface<ID3D11Texture2D>(bbuftex.GetAddressOf());
			D3D11_TEXTURE2D_DESC texdesc;
			bbuftex->GetDesc(&texdesc);
			D3D11_RENDER_TARGET_VIEW_DESC desc;
			desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			desc.Format = texdesc.Format;
			desc.Texture2D.MipSlice = 0;

			device11->CreateRenderTargetView(renderTargets11[i].Get(), &desc, defaultRtvs11[i].GetAddressOf());

			DX_API("wrap 12 depth buffer for d3d11")
				device11on12->CreateWrappedResource(
					depthStencilBuffers[i].Get(),
					&d3d11DepthFlags,
					D3D12_RESOURCE_STATE_DEPTH_WRITE,
					D3D12_RESOURCE_STATE_DEPTH_WRITE,
					IID_PPV_ARGS(depthStencils11[i].GetAddressOf())
				);

			CD3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc(D3D11_DSV_DIMENSION_TEXTURE2D);
			device11->CreateDepthStencilView(
				depthStencils11[i].Get(),
				&depthStencilViewDesc,
				defaultDsvs11[i].GetAddressOf()
			);
		}
		*/

		computeCommandLists.resize(swapChainBackBufferCount);
		computeAllocators.resize(swapChainBackBufferCount);
		for (uint i = 0; i < swapChainBackBufferCount; i++) {
			// Create compute allocator, command queue and command list
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

		copyCommandLists.resize(swapChainBackBufferCount);
		copyAllocators.resize(swapChainBackBufferCount);
		for (uint i = 0; i < swapChainBackBufferCount; i++) {
			// Create compute allocator, command queue and command list
			DX_API("create compute command allocator.")
				device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
					IID_PPV_ARGS(copyAllocators[i].ReleaseAndGetAddressOf()));

			DX_API("create compute command list.")
				device->CreateCommandList(
					0,
					D3D12_COMMAND_LIST_TYPE_DIRECT,
					copyAllocators[i].Get(),
					nullptr,
					IID_PPV_ARGS(copyCommandLists[i].ReleaseAndGetAddressOf()));
			copyCommandLists[i]->Close();
		}
	}

	void recordComputeCommands() {
		ID3D12DescriptorHeap* pHeaps[] = { uavHeap.Get() };
		computeCommandLists[swapChainBackBufferIndex]->SetDescriptorHeaps(_countof(pHeaps), pHeaps);

		if (frameCount > 0) {
			fillPins.populate(computeCommandLists[swapChainBackBufferIndex]);
			mortonSort.populate(computeCommandLists[swapChainBackBufferIndex]);
			mortonCountStarters.populate(computeCommandLists[swapChainBackBufferIndex]);
			clearHashList.populate(computeCommandLists[swapChainBackBufferIndex]);
			createCellList.populate(computeCommandLists[swapChainBackBufferIndex]);
			hashSort.populate(computeCommandLists[swapChainBackBufferIndex]);
			hashCountStarters.populate(computeCommandLists[swapChainBackBufferIndex]);
			clearHashLut.populate(computeCommandLists[swapChainBackBufferIndex]);
			createHashList.populate(computeCommandLists[swapChainBackBufferIndex]);
		} else {
			fillSortedPins.populate(computeCommandLists[swapChainBackBufferIndex]);
		}
	}

	void recordCopyCommands() {}

	virtual void PopulateCommandList() override {

		copyFenceChain.gpuWait(computeCommandQueue, previousSwapChainBackBufferIndex);

		computeAllocators[swapChainBackBufferIndex]->Reset();
		auto& computeCommandList = computeCommandLists[swapChainBackBufferIndex];
		computeCommandList->Reset(computeAllocators[swapChainBackBufferIndex].Get(), nullptr);

		recordComputeCommands();

		DX_API("close command list")
			computeCommandLists[swapChainBackBufferIndex]->Close();
		{
			// execute compute
			ID3D12CommandList* ppCommandLists[] = { computeCommandLists[swapChainBackBufferIndex].Get() };
			computeCommandQueue->ExecuteCommandLists(1, ppCommandLists);
		}
		computeFenceChain.signal(computeCommandQueue, swapChainBackBufferIndex);

		copyAllocators[swapChainBackBufferIndex]->Reset();
		auto& copyCommandList = copyCommandLists[swapChainBackBufferIndex];
		copyCommandList->Reset(copyAllocators[swapChainBackBufferIndex].Get(), nullptr);

		recordCopyCommands();

		//wait for compute to finish
		computeFenceChain.gpuWait(commandQueue, swapChainBackBufferIndex);

		DX_API("close command list")
			copyCommandLists[swapChainBackBufferIndex]->Close();
		{
			// execute copy
			ID3D12CommandList* ppCommandLists[] = { copyCommandLists[swapChainBackBufferIndex].Get() };
			commandQueue->ExecuteCommandLists(1, ppCommandLists);
		}
		copyFenceChain.signal(commandQueue, swapChainBackBufferIndex);

		commandAllocators[swapChainBackBufferIndex]->Reset();
		auto& commandList = commandLists[swapChainBackBufferIndex];
		commandList->Reset(commandAllocators[swapChainBackBufferIndex].Get(), nullptr);

		commandList->RSSetViewports(1, &viewPort);
		commandList->RSSetScissorRects(1, &scissorRect);

		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[swapChainBackBufferIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

		CD3DX12_CPU_DESCRIPTOR_HANDLE rHandle(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), swapChainBackBufferIndex, rtvDescriptorHandleIncrementSize);
		CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(dsvHeap->GetCPUDescriptorHandleForHeapStart());
		commandList->OMSetRenderTargets(1, &rHandle, FALSE, &dsvHandle);

		const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
		commandList->ClearRenderTargetView(rHandle, clearColor, 0, nullptr);
		commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		//READBACK HERE?
		for (auto name : { BUFFERNAMES }) {
			buffers[name].copyBack(commandLists[swapChainBackBufferIndex]);
		}

		//TEST HERE
		//for (auto name : { BUFFERNAMES }) {
		//	buffers[name].mapReadback();
		//}
		if (frameCount > 1)
		{
			uint* pMortons = buffers[mortons].mapReadback();
			bool ok = true;
			for (uint i = 0; i < 32; i++) {
				ok = ok && std::is_sorted(pMortons + i * 32 * 32, pMortons + i * 32 * 32 + 32 * 32
					//, MaskedComp(0x01160b00)
					, MortonComp()
					//TODO mortoncomp
				);
			}
			buffers[mortons].unmapReadback();
		}
		//		buffers[pins].mapReadback();
		//	
		if(frameCount > 1)
		{
			uint* pMortonPerPageBucketCounts = buffers[mortonPerPageBucketCounts].mapReadback();

			uint* pSortedPins = buffers[sortedPins].mapReadback();
			uint* pSortedMortons = buffers[sortedMortons].mapReadback();
			bool ok = std::is_sorted(pSortedMortons, pSortedMortons + 32 * 32 * 32
				, MaskedComp(0xffffffff)
				//, MaskedComp(0x01160b00)
				//, MortonComp()
				//TODO mortoncomp
			);

			uint* pMortonStarterCount = buffers[sortedMortonPerPageStarterCounts].mapReadback();
			//TODO verify startercount
			bool scok = verifyStarterCount(pSortedMortons, pMortonStarterCount);

			uint* pHlist = buffers[hashList].mapReadback();
			uint* pCellLut = buffers[cellLut].mapReadback();
			uint* pSortedCellLut = buffers[sortedCellLut].mapReadback();

			uint* pSortedHlist = buffers[sortedHashList].mapReadback();
			bool hok = std::is_sorted(pSortedHlist, pSortedHlist + 32 * 32 * 32);

			uint* pHlistStarters = buffers[sortedHashPerPageStarterCounts].mapReadback();
			bool hscok = verifyStarterCount(pSortedHlist, pHlistStarters);
			uint* pHashLut = buffers[hashLut].mapReadback();

			//hash to cell
			for (int i = 0; i < 32 * 32 * 32; i++) {
				uint hl = pHashLut[i];
				uint hStart = hl & 0xffff;
				uint hLength = hl >> 16;
				for (int ih = hStart; ih < hStart + hLength; ih++) {
					uint cl = pSortedCellLut[ih];
					uint cStart = cl & 0xffff;
					uint cLength = cl >> 16;
					for (int ip = cStart; ip < cStart + cLength; ip++) {
						if (i != (pSortedMortons[ip] * 499) % 32749)
//						if (i != pSortedMortons[ip])
							bool gaz = true;
					}
				}
			}
			//cell to hash
			for (int i = 0; i < 32 * 32 * 32; i++) {
				uint morton = pSortedMortons[i];
				if (morton == 0xffffffff)
					break;
				uint hash = (morton * 499) % 32749;
//				uint hash = morton;
				uint hl = pHashLut[hash];
				uint hStart = hl & 0xffff;
				uint hLength = hl >> 16;
				bool found = false;
				for (int ih = hStart; ih < hStart + hLength; ih++) {
					uint cl = pSortedCellLut[ih];
					uint cStart = cl & 0xffff;
					uint cLength = cl >> 16;
					if (cStart <= i && i < cStart + cLength) {
						found = true;
					}
				}
				if (!found)
					bool gaz = true;
			}

			buffers[mortonPerPageBucketCounts].unmapReadback();
			buffers[sortedPins].unmapReadback();
			buffers[hashLut].unmapReadback();
			buffers[sortedHashPerPageStarterCounts].unmapReadback();
			buffers[sortedHashList].unmapReadback();
			buffers[cellLut].unmapReadback();
			buffers[sortedCellLut].unmapReadback();
			buffers[hashList].unmapReadback();
			buffers[sortedMortonPerPageStarterCounts].unmapReadback();
			buffers[sortedMortons].unmapReadback();
		}

		DX_API("close command list")
			commandList->Close();

		ID3D12CommandList* cLists[] = { commandList.Get() };
		commandQueue->ExecuteCommandLists(_countof(cLists), cLists);

		graphicsFenceChain.signal(commandQueue, swapChainBackBufferIndex);

		WaitForPreviousFrame();

		frameCount++;

	}

	virtual void Resize(int width, int height) override {
		SimpleApp::Resize(width, height);

	}

	virtual void Update(float dt, float T) override {
		//noelf app11->animate(dt, T);
	}

	virtual void ProcessMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override {
		if (uMsg != 18) {
			//noelf app11->processMessage(hWnd, uMsg, wParam, lParam);
		}
	}

	virtual void ReleaseSwapChainResources() {
		__super::ReleaseSwapChainResources();
	}

	virtual void ReleaseResources() override {
		uavHeap.Reset();

		Egg::SimpleApp::ReleaseResources();
	}

};
