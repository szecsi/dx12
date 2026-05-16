#pragma once
#include "Egg/Common.h"
#include <Egg/Script/ScriptedApp.h>
#include <d3d11on12.h>
#include <algorithm>
#include <unordered_map>

#include "Egg/Compute/RawBuffer.h"
#include "Egg/Compute/TypedBuffer.h"
#include "Egg/Compute/ComputePass.h"
#include "Egg/Compute/FenceChain.h"
#include "Egg/Shader.h"

#include "Shaders/TreeCB.hlsli"
#include "Egg/Script/StructReflectionMap.h"
#include "Egg/ConstantBuffer.hpp"

#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

class TreeApp : public Egg::Script::ScriptedApp {

	uint frameCount;

	Egg::ConstantBuffer<TreeMaterialCb> treeMaterialCb;

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
		auto* self = reinterpret_cast<TreeApp*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
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
		wc.lpszClassName = L"TreeGuiPanel";
		RegisterClassExW(&wc);

		guiHwnd = CreateWindowExW(WS_EX_TOOLWINDOW, L"TreeGuiPanel", L"TreeMaterialCb",
			WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
			544, 0, 300, 200, nullptr, nullptr, GetModuleHandle(nullptr), this);

		auto& registry = Egg::Script::StructReflectionMap::getMap();
		auto it = registry.find("TreeMaterialCb");
		if (it == registry.end()) return;

		const int RH = 30, LW = 110, SW = 130, VW = 58, M = 8;
		int y = M;

		for (auto& member : it->second) {
			float minVal = 0.f, maxVal = 1.f;
			//if (member.name == "lineSize")   { minVal = 0.f; maxVal = 2.f; }
			
			int wlen = MultiByteToWideChar(CP_ACP, 0, member.name.c_str(), -1, nullptr, 0);
			std::wstring wname(wlen, 0);
			MultiByteToWideChar(CP_ACP, 0, member.name.c_str(), -1, &wname[0], wlen);

			int n = (int)(member.size / sizeof(float));
			for (int i = 0; i < n; i++) {
				float* ptr = reinterpret_cast<float*>(
					reinterpret_cast<char*>(&treeMaterialCb.data) + member.offset + i * sizeof(float));

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

	/*
	* LOD classes:
	*	- animated trees 0/1/2
	*	- billboard trees
	*	- billboard groves
	* y0/y1/y2/i0/i1/i2/psi0/psi1/psi2 instance buffers for visible pieces
	*	two/three/four global bone indices
	*	pre-determined size
	*	pre-determined partitioning per tree
	* bone buffer for animated trees
	*	dqs
	*	
	* joint buffer for animated trees
	* tree modeldq buffer to place trees on ground
	*
	* 32*27=864 different tree models
	* piece counts from 33 to 255 step 1/3
	* 124k pieces per single instace set of models
	* 9M pieces total
	* model consists of
	*	bone trafos
	*	branch twists
	* for one tree model 8*9=72 instances:
	*	4 level-2 instances
	*	12 level-1 instances
	*	56 level-0 instances
	*	located at 2x3 Halton sequence points
	*	first 72 for first model, next 72 for second model, etc.
	* 
	* growGS -- run at init
	*	if close, leave it alone
	*	fills in instance buffers 
	* 
	* sortGS
	*   sort 1M trees by visibility
	* 
	* scanGS
	*	determine tree bone offsets
	* 
	* treeherdGS
	*	populates visible area
	*		by replanting trees
	*		if not in proper zone
	*   writes tree model buffer

	* 
	* windGS
	*	from joint buffer, build bone buffer
	* 
	* treeVS
	*	gets instance data: global bone indices
	*	use bone dqs from bone buffer * tree model matrix
	* 
	* Tree Procedure
	*    from id
	*		hash piece counts
	*		arrange set of pieces hash-randomly
	*		set hash-random joints, avoid collisions
	*	write out joint data
	*/

	static const unsigned int nLevels = 1; // TODO: 3
	static const unsigned int nBranchTypes = 1; // TODO: 3
	static const unsigned int nTreeModels = 32 * 27;
	static const unsigned int nInstancesPerTreeModel = 8 * 9;
	static const unsigned int nAveragePieces = (16+128) / 2;

	com_ptr<ID3D12DescriptorHeap> uavHeap;
	Egg::Compute::RawBuffer   instanceBuffers[nLevels * nBranchTypes];
	Egg::Compute::TypedBuffer bonesBuffer;

	com_ptr<ID3D12Resource>          dispatchArgsResource;
	com_ptr<ID3D12CommandSignature>  dispatchCommandSignature;

	Egg::Compute::ComputeShader growCS;
	Egg::Compute::ComputeShader treeherdCS;

	com_ptr<ID3D12CommandSignature>  drawCommandSignature;

public:
	TreeApp() : ScriptedApp(),
		instanceBuffers {
			Egg::Compute::RawBuffer(L"y2",
				nTreeModels * nAveragePieces * 3u)
		},
		bonesBuffer(L"bones", nTreeModels * nAveragePieces * 2u/*nChildren*/ * 2u/*dq*/, DXGI_FORMAT_R32G32B32A32_FLOAT)
	{}

	virtual void CreateResources() override {
		Egg::Script::ScriptedApp::CreateResources();

		frameCount = 0;
		uploadFence.createResources(device);

		D3D12_DESCRIPTOR_HEAP_DESC dhd;
		dhd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		dhd.NodeMask = 0;
		dhd.NumDescriptors = 12;
		dhd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		uint dhIncrSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		DX_API("create descriptor heap for uavs")
			device->CreateDescriptorHeap(&dhd, IID_PPV_ARGS(uavHeap.GetAddressOf()));


		CD3DX12_CPU_DESCRIPTOR_HANDLE CountsHandle(uavHeap->GetCPUDescriptorHandleForHeapStart(), 0, dhIncrSize);
		instanceBuffers[0].createResources(device, CountsHandle);
		CD3DX12_CPU_DESCRIPTOR_HANDLE fragmentsHandle(uavHeap->GetCPUDescriptorHandleForHeapStart(), 1, dhIncrSize);
		bonesBuffer.createResources(device, fragmentsHandle);

		growCS.createResources(device, "Shaders/growCS.cso");
		treeherdCS.createResources(device, "Shaders/treeherdCS.cso");

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

		treeMaterialCb.CreateResources(device.Get());

		//treeMaterialCb.data.lineSize    = { 0.2f, 0.06f };
		//treeMaterialCb.data.fading      = { 1.0f, 1.0f };
		//treeMaterialCb.data.texScale    = { 0.3f, 0.3f, 0.3f, 0.3f };
		//treeMaterialCb.data.crossAngle  = { 0.0f, 0.125f, 0.25f, 0.375f };
		//treeMaterialCb.data.stripWidth  = 0.005f;
		//treeMaterialCb.data.overdraw    = 1.0f;

		RunScript("scene.lua");

		//GG_STRUCT(TreeMaterialCb)
		//	GG_MEMBER(lineSize)
		//	GG_MEMBER(fading)
		//	GG_MEMBER(texScale)
		//	GG_MEMBER(crossAngle)
		//	GG_MEMBER(stripWidth)
		//	GG_MEMBER(overdraw)
		//GG_ENDSTRUCT;

		auto matIt = guiMaterials.find("TreeMaterialCb");
		if (matIt != guiMaterials.end())
			matIt->second->SetConstantBuffer(treeMaterialCb);

		LoadTexture2D("tree/bark-alpha-tiling.png").CreateSRV(device.Get(), uavHeap.Get(), 3);

		SceneUploadResources();

	}

	void recordComputeCommands() {
		auto& cmd = computeCommandLists[swapChainBackBufferIndex];
		ID3D12DescriptorHeap* pHeaps[] = { uavHeap.Get() };
		cmd->SetDescriptorHeaps(_countof(pHeaps), pHeaps);
		D3D12_GPU_DESCRIPTOR_HANDLE heap0 = uavHeap->GetGPUDescriptorHandleForHeapStart();


		growCS.setup(cmd, heap0, 0);
		cmd->Dispatch(nTreeModels, 1, 1);
		cmd->ResourceBarrier(1, &instanceBuffers[0].uavBarrier());

	}

	virtual void PopulateCommandList() override {

		// --- Compute pass ---
		computeAllocators[swapChainBackBufferIndex]->Reset();
		auto& computeCommandList = computeCommandLists[swapChainBackBufferIndex];
		computeCommandList->Reset(computeAllocators[swapChainBackBufferIndex].Get(), nullptr);

		recordComputeCommands();

		DX_API("close compute command list")
			computeCommandLists[swapChainBackBufferIndex]->Close();

		graphicsFenceChain.gpuWait(computeCommandQueue, swapChainBackBufferIndex);
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

		{
			D3D12_RESOURCE_BARRIER barriers[] = {
				CD3DX12_RESOURCE_BARRIER::Transition(instanceBuffers[0].getResource(),
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
					D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
				CD3DX12_RESOURCE_BARRIER::Transition(bonesBuffer.getResource(),
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
					D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
			};
			commandList->ResourceBarrier(2, barriers);
		}

		CD3DX12_CPU_DESCRIPTOR_HANDLE rHandle(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), swapChainBackBufferIndex, rtvDescriptorHandleIncrementSize);
		CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(dsvHeap->GetCPUDescriptorHandleForHeapStart());

		const float clearColor[] = { 0.5f, 0.6f, 0.9f, 1.0f };
		commandList->ClearRenderTargetView(rHandle, clearColor, 0, nullptr);
		commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		__super::PopulateCommandList();

		{
			D3D12_RESOURCE_BARRIER barriers[] = {
				CD3DX12_RESOURCE_BARRIER::Transition(instanceBuffers[0].getResource(),
					D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS
					),
				CD3DX12_RESOURCE_BARRIER::Transition(bonesBuffer.getResource(),
					D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS
					),
			};
			commandList->ResourceBarrier(2, barriers);
		}
		
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
		hipHop.update(T, &perObjectCb->objects[hipHop.getBaseObjectIndex()]);
		__super::Update(dt, T);
		retamMaterialCb.Upload();
	}

	virtual void ProcessMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override {
		if (!guiHwnd) createGui();
		else if (uMsg == WM_KEYUP) {
			if (wParam == 49) {
				showRetam = !showRetam;
			}
			if (wParam == 50) {
				showStrokes = !showStrokes;
			}
			if (wParam == 51) {
				showReCollect = !showReCollect;
			}
			if (wParam == 52) {
				showAnim = !showAnim;
			}
		}

		__super::ProcessMessage(hWnd, uMsg, wParam, lParam);
	}

	virtual void ReleaseSwapChainResources() override {
		collectDepthBuffer.Reset();
		collectDsvHeap.Reset();
		collectColorBuffer.Reset();
		collectRtvHeap.Reset();
		__super::ReleaseSwapChainResources();
	}

	virtual void ReleaseResources() override {
		if (guiHwnd) { DestroyWindow(guiHwnd); guiHwnd = nullptr; }
		hipHop.releaseResources();
		retamMaterialCb.ReleaseResources();
		fragmentCountsBuffer.releaseResources();
		fragmentsBuffer.releaseResources();
		designBuffer.releaseResources();
		strokeListBuffer.releaseResources();
		dispatchArgsResource.Reset();
		dispatchCommandSignature.Reset();
		drawCommandSignature.Reset();
		cubicExtrudePSO.Reset();
		cubicExtrudeRootSig.Reset();
		uavHeap.Reset();
		Egg::Script::ScriptedApp::ReleaseResources();
	}
};
