#pragma once
#include "Egg/Common.h"
#include <Egg/Script/ScriptedApp.h>
#include <d3d11on12.h>
#include <algorithm>

#include "Egg/Compute/RawBuffer.h"
#include "Egg/Compute/TypedBuffer.h"
#include "Egg/Compute/ComputePass.h"
#include "Egg/Compute/FenceChain.h"

#include "Shaders/Retam/RetamCb.hlsli"
#include "Egg/Script/StructReflectionMap.h"
#include "Egg/ConstantBuffer.hpp"

#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

class RetamApp : public Egg::Script::ScriptedApp {

	uint frameCount;

	Egg::ConstantBuffer<RetamMaterialCb> retamMaterialCb;

	struct FloatControl {
		HWND slider;
		HWND valueLabel;
		float* ptr;
		float minVal, maxVal;
	};
	static constexpr int GUI_STEPS = 1000;
	HWND guiHwnd = nullptr;
	std::vector<FloatControl> floatControls;

	static LRESULT CALLBACK GuiWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
		if (msg == WM_CREATE) {
			SetWindowLongPtr(hwnd, GWLP_USERDATA,
				reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCT*>(lp)->lpCreateParams));
			return 0;
		}
		auto* self = reinterpret_cast<RetamApp*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
		if (msg == WM_HSCROLL && self) {
			HWND hCtrl = reinterpret_cast<HWND>(lp);
			for (auto& fc : self->floatControls) {
				if (fc.slider == hCtrl) {
					int pos = (int)SendMessage(hCtrl, TBM_GETPOS, 0, 0);
					*fc.ptr = fc.minVal + (fc.maxVal - fc.minVal) * pos / (float)GUI_STEPS;
					wchar_t buf[32];
					swprintf_s(buf, L"%.3f", *fc.ptr);
					SetWindowTextW(fc.valueLabel, buf);
					break;
				}
			}
			return 0;
		}
		if (msg == WM_CLOSE) { ShowWindow(hwnd, SW_HIDE); return 0; }
		return DefWindowProcW(hwnd, msg, wp, lp);
	}

	void createGui() {
		INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_BAR_CLASSES };
		InitCommonControlsEx(&icc);

		WNDCLASSEXW wc = {};
		wc.cbSize = sizeof(wc);
		wc.lpfnWndProc = GuiWndProc;
		wc.hInstance = GetModuleHandle(nullptr);
		wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
		wc.lpszClassName = L"RetamGuiPanel";
		RegisterClassExW(&wc);

		guiHwnd = CreateWindowExW(WS_EX_TOOLWINDOW, L"RetamGuiPanel", L"RetamMaterialCb",
			WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
			544, 0, 300, 200, nullptr, nullptr, GetModuleHandle(nullptr), this);

		auto& registry = Egg::Script::StructReflectionMap::getMap();
		auto it = registry.find("RetamMaterialCb");
		if (it == registry.end()) return;

		const int RH = 30, LW = 110, SW = 130, VW = 58, M = 8;
		int y = M;

		for (auto& member : it->second) {
			float minVal = 0.f, maxVal = 1.f;
			if (member.name == "lineSize")   { minVal = 0.f; maxVal = 2.f; }
			if (member.name == "fading")	 { minVal = 0.f; maxVal = 1.f; }
			if (member.name == "texScale")   { minVal = 0.f; maxVal = 10.f; }
			if (member.name == "crossAngle") { minVal = 0.f; maxVal = 6.2832f; }

			int wlen = MultiByteToWideChar(CP_ACP, 0, member.name.c_str(), -1, nullptr, 0);
			std::wstring wname(wlen, 0);
			MultiByteToWideChar(CP_ACP, 0, member.name.c_str(), -1, &wname[0], wlen);

			int n = (int)(member.size / sizeof(float));
			for (int i = 0; i < n; i++) {
				float* ptr = reinterpret_cast<float*>(
					reinterpret_cast<char*>(&retamMaterialCb.data) + member.offset + i * sizeof(float));

				std::wstring label = wname + L"." + (wchar_t)('x' + i);
				CreateWindowW(L"STATIC", label.c_str(), WS_CHILD | WS_VISIBLE | SS_RIGHT,
					M, y + 6, LW, 20, guiHwnd, nullptr, nullptr, nullptr);

				HWND slider = CreateWindowW(TRACKBAR_CLASSW, nullptr,
					WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
					M + LW + 4, y, SW, RH, guiHwnd, nullptr, nullptr, nullptr);
				SendMessage(slider, TBM_SETRANGE, TRUE, MAKELONG(0, GUI_STEPS));
				int initPos = (int)((*ptr - minVal) / (maxVal - minVal) * GUI_STEPS + 0.5f);
				SendMessage(slider, TBM_SETPOS, TRUE, std::max(0, std::min(GUI_STEPS, initPos)));

				wchar_t valBuf[32];
				swprintf_s(valBuf, L"%.3f", *ptr);
				HWND valLabel = CreateWindowW(L"STATIC", valBuf, WS_CHILD | WS_VISIBLE,
					M + LW + 4 + SW + 4, y + 6, VW, 20, guiHwnd, nullptr, nullptr, nullptr);

				floatControls.push_back({ slider, valLabel, ptr, minVal, maxVal });
				y += RH + 4;
			}
		}

		RECT rc = { 0, 0, M + LW + 4 + SW + 4 + VW + M, y + 32 };
		AdjustWindowRectEx(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_TOOLWINDOW);
		SetWindowPos(guiHwnd, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOMOVE | SWP_NOZORDER);
	}

protected:

	com_ptr<ID3D12CommandQueue>  computeCommandQueue;
	std::vector < com_ptr<ID3D12CommandAllocator> > computeAllocators;
	std::vector < com_ptr<ID3D12GraphicsCommandList> > computeCommandLists;

	com_ptr<ID3D12CommandAllocator> uploadAllocator;
	com_ptr<ID3D12GraphicsCommandList> uploadCommandList;

	Egg::Compute::Fence uploadFence;
	Egg::Compute::FenceChain computeFenceChain;
	Egg::Compute::FenceChain graphicsFenceChain;

	com_ptr<ID3D12Resource> collectDepthBuffer;
	com_ptr<ID3D12DescriptorHeap> collectDsvHeap;

	com_ptr<ID3D12DescriptorHeap> uavHeap;
	Egg::Compute::RawBuffer   fragmentCountsBuffer;
	Egg::Compute::TypedBuffer fragmentsBuffer;
	Egg::Compute::TypedBuffer designBuffer;
	Egg::Compute::RawBuffer   strokeCountsBuffer;
	Egg::Compute::RawBuffer   debugBuffer;

	Egg::Compute::ComputeShader sortCS;
	//D3D12_RESOURCE_BARRIER uavBarrier;

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

public:
	RetamApp() : ScriptedApp(),
		fragmentCountsBuffer(L"fragmentCounts", 16 * 1024),
		fragmentsBuffer(L"fragments", 1024u * 1024u * 16u),
		designBuffer(L"design", 8u * 5u * 1024u * 16u, DXGI_FORMAT_R32G32B32A32_FLOAT),
		strokeCountsBuffer(L"strokeCounts", 1024u * 16u),
		debugBuffer(L"debug", 1024u * 1024u * 16u)
	{}

	virtual void CreateResources() override {
		Egg::Script::ScriptedApp::CreateResources();

		frameCount = 0;

		uploadFence.createResources(device);

		D3D12_DESCRIPTOR_HEAP_DESC dhd;
		dhd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		dhd.NodeMask = 0;
		dhd.NumDescriptors = 5;
		dhd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		uint dhIncrSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		DX_API("create descriptor heap for uavs")
			device->CreateDescriptorHeap(&dhd, IID_PPV_ARGS(uavHeap.GetAddressOf()));


		CD3DX12_CPU_DESCRIPTOR_HANDLE CountsHandle(uavHeap->GetCPUDescriptorHandleForHeapStart(), 0, dhIncrSize);
		fragmentCountsBuffer.createResources(device, CountsHandle);
		CD3DX12_CPU_DESCRIPTOR_HANDLE fragmentsHandle(uavHeap->GetCPUDescriptorHandleForHeapStart(), 1, dhIncrSize);
		fragmentsBuffer.createResources(device, fragmentsHandle);
		CD3DX12_CPU_DESCRIPTOR_HANDLE designHandle(uavHeap->GetCPUDescriptorHandleForHeapStart(), 2, dhIncrSize);
		designBuffer.createResources(device, designHandle);
		CD3DX12_CPU_DESCRIPTOR_HANDLE strokeCountsHandle(uavHeap->GetCPUDescriptorHandleForHeapStart(), 3, dhIncrSize);
		strokeCountsBuffer.createResources(device, strokeCountsHandle);
		CD3DX12_CPU_DESCRIPTOR_HANDLE debugHandle(uavHeap->GetCPUDescriptorHandleForHeapStart(), 4, dhIncrSize);
		debugBuffer.createResources(device, debugHandle);

		sortCS.createResources(device, "Shaders/Retam/sortCS.cso");

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

		D3D12_DESCRIPTOR_HEAP_DESC collectDsvHeapDesc = {};
		collectDsvHeapDesc.NumDescriptors = 1;
		collectDsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		collectDsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		DX_API("create collect DSV heap")
			device->CreateDescriptorHeap(&collectDsvHeapDesc, IID_PPV_ARGS(collectDsvHeap.ReleaseAndGetAddressOf()));

		D3D12_CLEAR_VALUE collectDepthClearVal = {};
		collectDepthClearVal.Format = DXGI_FORMAT_D32_FLOAT;
		collectDepthClearVal.DepthStencil.Depth = 1.0f;
		DX_API("create collect depth buffer")
			device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, 1024, 1024, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
				D3D12_RESOURCE_STATE_DEPTH_WRITE,
				&collectDepthClearVal,
				IID_PPV_ARGS(collectDepthBuffer.ReleaseAndGetAddressOf()));
		collectDepthBuffer->SetName(L"Collect Depth Buffer");

		D3D12_DEPTH_STENCIL_VIEW_DESC collectDsvDesc = {};
		collectDsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
		collectDsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		collectDsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		device->CreateDepthStencilView(collectDepthBuffer.Get(), &collectDsvDesc, collectDsvHeap->GetCPUDescriptorHandleForHeapStart());
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

		retamMaterialCb.CreateResources(device.Get());

		retamMaterialCb.data.lineSize    = { 0.6f, 0.2f };
		retamMaterialCb.data.fading      = { 1.0f, 1.0f };
		retamMaterialCb.data.texScale    = { 2.0f, 2.0f, 2.0f, 2.0f };
		retamMaterialCb.data.crossAngle  = { 0.0f, 0.125f, 0.25f, 0.375f };

		RunScript("scene.lua");

		GG_STRUCT(RetamMaterialCb)
			GG_MEMBER(lineSize)
			GG_MEMBER(fading)
			GG_MEMBER(texScale)
			GG_MEMBER(crossAngle)
		GG_ENDSTRUCT;

		auto matIt = guiMaterials.find("RetamMaterialCb");
		if (matIt != guiMaterials.end())
			matIt->second->SetConstantBuffer(retamMaterialCb);

		auto collectIt = guiMaterials.find("retam256Collect");
		if (collectIt != guiMaterials.end()) {
			collectIt->second->SetConstantBuffer(retamMaterialCb);
			collectIt->second->SetSrvHeap(3, uavHeap, 0);
			collectIt->second->depthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		}

		auto depthIt = guiMaterials.find("layDownDepth");
		//if (depthIt != guiMaterials.end())
		//	depthIt->second->SetConstantBuffer(retamMaterialCb);

		SceneUploadResources();
	}

	void recordComputeCommands() {
		ID3D12DescriptorHeap* pHeaps[] = { uavHeap.Get() };
		computeCommandLists[swapChainBackBufferIndex]->SetDescriptorHeaps(_countof(pHeaps), pHeaps);

		sortCS.setup(computeCommandLists[swapChainBackBufferIndex], uavHeap->GetGPUDescriptorHandleForHeapStart(), 0);
		computeCommandLists[swapChainBackBufferIndex]->Dispatch(1024*16, 1, 1);
		computeCommandLists[swapChainBackBufferIndex]->ResourceBarrier(1, &debugBuffer.uavBarrier());

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

		if (frameCount > 1) {
			// check compute results
/*			uint* fragmentsBufferData = (uint*)fragmentsBuffer.mapReadback();
			uint* fragmentCountsData = fragmentCountsBuffer.mapReadback();
			uint* strokeCountsData = strokeCountsBuffer.mapReadback();

			for (int si = 0; si < 1024 * 16; si++) {
				uint count = fragmentCountsData[si];
				if (count > 0) {
					printf("Stroke %d: %d fragments\n", si, count);
					uint* con = fragmentsBufferData + si * 1024 * 4;

					for (int i = 0; i < count; i++) {
						uint f = con[i];
						uint x = f & 0xFFFF;
						uint y = (f >> 16) & 0xFFFF;
						printf("\tFragment %d: (%d, %d)\n", i, x, y);
					}

				}
			}*/

			uint* debugData = debugBuffer.mapReadback();
			debugBuffer.unmapReadback();
/*			strokeCountsBuffer.unmapReadback();
			fragmentCountsBuffer.unmapReadback();
			fragmentsBuffer.unmapReadback();*/
		}

		// --- Graphics/render pass ---
		commandAllocator->Reset();
		commandList->Reset(commandAllocator.Get(), nullptr);

		CD3DX12_CPU_DESCRIPTOR_HANDLE rHandle(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), swapChainBackBufferIndex, rtvDescriptorHandleIncrementSize);
		CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(dsvHeap->GetCPUDescriptorHandleForHeapStart());
		CD3DX12_CPU_DESCRIPTOR_HANDLE collectDsvHandle(collectDsvHeap->GetCPUDescriptorHandleForHeapStart());

		D3D12_VIEWPORT collectViewPort = { 0.0f, 0.0f, 1024.0f, 1024.0f, 0.0f, 1.0f };
		D3D12_RECT collectScissor = { 0, 0, 1024, 1024 };
		commandList->RSSetViewports(1, &collectViewPort);
		commandList->RSSetScissorRects(1, &collectScissor);

		// Clear counts and lay down depth into 1024x1024 depth buffer
		fragmentCountsBuffer.upload(commandList);
		commandList->OMSetRenderTargets(0, nullptr, FALSE, &collectDsvHandle);
		commandList->ClearDepthStencilView(collectDsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		for (int i = 0; i < (int)entities.size(); i++)
			entities[i]->Draw(commandList.Get(), 2, i);

		// Collect pass — depth test, no depth write, writes to UAV buffers
		for (int i = 0; i < (int)entities.size(); i++)
			entities[i]->Draw(commandList.Get(), 1, i);

		//commandList->ResourceBarrier(1, &fragmentCountsBuffer.uavBarrier());

		//fragmentCountsBuffer.copyBack(commandList);
		//fragmentsBuffer.copyBack(commandList);
		debugBuffer.copyBack(commandList);

		// Restore main viewport and render to swap chain
		commandList->RSSetViewports(1, &viewPort);
		commandList->RSSetScissorRects(1, &scissorRect);
		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[swapChainBackBufferIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
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
		retamMaterialCb.Upload();
	}

	virtual void ProcessMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override {
		if (!guiHwnd) createGui();
		__super::ProcessMessage(hWnd, uMsg, wParam, lParam);
	}

	virtual void ReleaseSwapChainResources() override {
		collectDepthBuffer.Reset();
		collectDsvHeap.Reset();
		__super::ReleaseSwapChainResources();
	}

	virtual void ReleaseResources() override {
		if (guiHwnd) { DestroyWindow(guiHwnd); guiHwnd = nullptr; }
		retamMaterialCb.ReleaseResources();
		fragmentCountsBuffer.releaseResources();
		fragmentsBuffer.releaseResources();
		designBuffer.releaseResources();
		uavHeap.Reset();
		Egg::Script::ScriptedApp::ReleaseResources();
	}
};
