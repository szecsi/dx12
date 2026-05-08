#pragma once
#include "Egg/Common.h"
#include <Egg/Script/ScriptedApp.h>
#include "HipHopAnimation.h"
#include <d3d11on12.h>
#include <algorithm>
#include <unordered_map>

#include "Egg/Compute/RawBuffer.h"
#include "Egg/Compute/TypedBuffer.h"
#include "Egg/Compute/ComputePass.h"
#include "Egg/Compute/FenceChain.h"
#include "Egg/Shader.h"

#include "Shaders/Retam/RetamCb.hlsli"
#include "Egg/Script/StructReflectionMap.h"
#include "Egg/ConstantBuffer.hpp"

#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

class RetamApp : public Egg::Script::ScriptedApp {

	uint frameCount;
	bool showStrokes;
	bool showRetam;
	bool showReCollect;
	bool showAnim;

	HipHopAnimation hipHop;

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
			if (member.name == "stripWidth") { minVal = 0.f; maxVal = 0.05f; }
			if (member.name == "overdraw")   { minVal = 0.f; maxVal = 3.f; }

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
	com_ptr<ID3D12Resource> collectColorBuffer;
	com_ptr<ID3D12DescriptorHeap> collectRtvHeap;

	com_ptr<ID3D12DescriptorHeap> uavHeap;
	Egg::Compute::RawBuffer   fragmentCountsBuffer;
	Egg::Compute::TypedBuffer fragmentsBuffer;
	Egg::Compute::TypedBuffer designBuffer;
	Egg::Compute::TypedBuffer cubicBuffer;
	Egg::Compute::RawBuffer   strokeCountsBuffer;
	Egg::Compute::RawBuffer   strokeOffsetsBuffer;
	Egg::Compute::RawBuffer   strokeListBuffer;
	Egg::Compute::RawBuffer   debugBuffer;

	com_ptr<ID3D12Resource>          dispatchArgsResource;
	com_ptr<ID3D12CommandSignature>  dispatchCommandSignature;

	Egg::Compute::ComputeShader sortCS;
	Egg::Compute::ComputeShader prefixSumCS;
	Egg::Compute::ComputeShader compactCS;
	Egg::Compute::ComputeShader argsCS;
	Egg::Compute::ComputeShader cubicCS;

	com_ptr<ID3D12CommandSignature>  drawCommandSignature;
	com_ptr<ID3D12RootSignature>     cubicExtrudeRootSig;
	com_ptr<ID3D12PipelineState>     cubicExtrudePSO;

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
		designBuffer(L"design", 16u * 5u * 1024u * 16u, DXGI_FORMAT_R32G32B32A32_FLOAT),
		cubicBuffer(L"cubic", 1024u * 16u * 16u * 4u, DXGI_FORMAT_R32G32B32A32_FLOAT),
		strokeCountsBuffer(L"strokeCounts", 1024u * 16u),
		strokeOffsetsBuffer(L"strokeOffsets", 1024u * 16u),
		strokeListBuffer(L"strokeList", 1024u * 16u * 16u),
		debugBuffer(L"debug", 1024u * 1024u * 16u)
	{}

	virtual void CreateResources() override {
		Egg::Script::ScriptedApp::CreateResources();

		frameCount = 0;
		showRetam = true;
		showStrokes = true;
		showReCollect = false;

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
		fragmentCountsBuffer.createResources(device, CountsHandle);
		CD3DX12_CPU_DESCRIPTOR_HANDLE fragmentsHandle(uavHeap->GetCPUDescriptorHandleForHeapStart(), 1, dhIncrSize);
		fragmentsBuffer.createResources(device, fragmentsHandle);
		CD3DX12_CPU_DESCRIPTOR_HANDLE designHandle(uavHeap->GetCPUDescriptorHandleForHeapStart(), 2, dhIncrSize);
		designBuffer.createResources(device, designHandle);
		CD3DX12_CPU_DESCRIPTOR_HANDLE strokeCountsHandle(uavHeap->GetCPUDescriptorHandleForHeapStart(), 3, dhIncrSize);
		strokeCountsBuffer.createResources(device, strokeCountsHandle);
		CD3DX12_CPU_DESCRIPTOR_HANDLE debugHandle(uavHeap->GetCPUDescriptorHandleForHeapStart(), 4, dhIncrSize);
		debugBuffer.createResources(device, debugHandle);
		CD3DX12_CPU_DESCRIPTOR_HANDLE strokeOffsetsHandle(uavHeap->GetCPUDescriptorHandleForHeapStart(), 5, dhIncrSize);
		strokeOffsetsBuffer.createResources(device, strokeOffsetsHandle);
		CD3DX12_CPU_DESCRIPTOR_HANDLE cubicHandle(uavHeap->GetCPUDescriptorHandleForHeapStart(), 6, dhIncrSize);
		cubicBuffer.createResources(device, cubicHandle);
		CD3DX12_CPU_DESCRIPTOR_HANDLE strokeListHandle(uavHeap->GetCPUDescriptorHandleForHeapStart(), 7, dhIncrSize);
		strokeListBuffer.createResources(device, strokeListHandle);

		{
			D3D12_RESOURCE_DESC argsDesc = CD3DX12_RESOURCE_DESC::Buffer(7 * sizeof(uint), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
			DX_API("create dispatch args buffer")
				device->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
					D3D12_HEAP_FLAG_NONE, &argsDesc,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
					IID_PPV_ARGS(dispatchArgsResource.GetAddressOf()));
			dispatchArgsResource->SetName(L"dispatchArgs");

			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
			uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			uavDesc.Buffer.NumElements = 7;
			uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
			CD3DX12_CPU_DESCRIPTOR_HANDLE argsHandle(uavHeap->GetCPUDescriptorHandleForHeapStart(), 8, dhIncrSize);
			device->CreateUnorderedAccessView(dispatchArgsResource.Get(), nullptr, &uavDesc, argsHandle);
		}

		{
			D3D12_INDIRECT_ARGUMENT_DESC argDesc = {};
			argDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
			D3D12_COMMAND_SIGNATURE_DESC csDesc = {};
			csDesc.ByteStride = sizeof(D3D12_DISPATCH_ARGUMENTS);
			csDesc.NumArgumentDescs = 1;
			csDesc.pArgumentDescs = &argDesc;
			DX_API("create dispatch command signature")
				device->CreateCommandSignature(&csDesc, nullptr, IID_PPV_ARGS(dispatchCommandSignature.GetAddressOf()));
		}

		{
			D3D12_INDIRECT_ARGUMENT_DESC argDesc = {};
			argDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
			D3D12_COMMAND_SIGNATURE_DESC csDesc = {};
			csDesc.ByteStride = sizeof(D3D12_DRAW_ARGUMENTS);
			csDesc.NumArgumentDescs = 1;
			csDesc.pArgumentDescs = &argDesc;
			DX_API("create draw command signature")
				device->CreateCommandSignature(&csDesc, nullptr, IID_PPV_ARGS(drawCommandSignature.GetAddressOf()));
		}

		{
			CD3DX12_CPU_DESCRIPTOR_HANDLE cubicSrvHandle(uavHeap->GetCPUDescriptorHandleForHeapStart(), 9, dhIncrSize);
			cubicBuffer.createSrv(device, cubicSrvHandle);
		}

		{
			com_ptr<ID3DBlob> vs = Egg::Shader::LoadCso("Shaders/Retam/extrudeCubicVS.cso");
			com_ptr<ID3DBlob> gs = Egg::Shader::LoadCso("Shaders/Retam/extrudeCubicGS.cso");
			com_ptr<ID3DBlob> ps = Egg::Shader::LoadCso("Shaders/Retam/extrudeCubicPS.cso");
			cubicExtrudeRootSig = Egg::Shader::LoadRootSignature(device.Get(), vs.Get());

			D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
			psoDesc.pRootSignature = cubicExtrudeRootSig.Get();
			psoDesc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
			psoDesc.GS = { gs->GetBufferPointer(), gs->GetBufferSize() };
			psoDesc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
			psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
			psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
			psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			psoDesc.BlendState.RenderTarget[0].BlendEnable           = TRUE;
			psoDesc.BlendState.RenderTarget[0].SrcBlend              = D3D12_BLEND_SRC_ALPHA;
			psoDesc.BlendState.RenderTarget[0].DestBlend             = D3D12_BLEND_INV_SRC_ALPHA;
			psoDesc.BlendState.RenderTarget[0].BlendOp               = D3D12_BLEND_OP_ADD;
			psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha         = D3D12_BLEND_ONE;
			psoDesc.BlendState.RenderTarget[0].DestBlendAlpha        = D3D12_BLEND_ZERO;
			psoDesc.BlendState.RenderTarget[0].BlendOpAlpha          = D3D12_BLEND_OP_ADD;
			psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
			psoDesc.NumRenderTargets = 1;
			psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
			psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
			psoDesc.SampleMask = UINT_MAX;
			psoDesc.SampleDesc.Count = 1;
			DX_API("create cubic extrude PSO")
				device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(cubicExtrudePSO.GetAddressOf()));
		}

		sortCS.createResources(device, "Shaders/Retam/sortCS.cso");
		prefixSumCS.createResources(device, "Shaders/Retam/prefixSumCS.cso");
		compactCS.createResources(device, "Shaders/Retam/compactCS.cso");
		argsCS.createResources(device, "Shaders/Retam/argsCS.cso");
		cubicCS.createResources(device, "Shaders/Retam/cubicCS.cso");

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

		D3D12_DESCRIPTOR_HEAP_DESC collectRtvHeapDesc = {};
		collectRtvHeapDesc.NumDescriptors = 1;
		collectRtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		collectRtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		DX_API("create collect RTV heap")
			device->CreateDescriptorHeap(&collectRtvHeapDesc, IID_PPV_ARGS(collectRtvHeap.ReleaseAndGetAddressOf()));

		D3D12_CLEAR_VALUE collectColorClearVal = {};
		collectColorClearVal.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		DX_API("create collect color buffer")
			device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, 1024, 1024, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				&collectColorClearVal,
				IID_PPV_ARGS(collectColorBuffer.ReleaseAndGetAddressOf()));
		collectColorBuffer->SetName(L"Collect Color Buffer");

		D3D12_RENDER_TARGET_VIEW_DESC collectRtvDesc = {};
		collectRtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		collectRtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		device->CreateRenderTargetView(collectColorBuffer.Get(), &collectRtvDesc, collectRtvHeap->GetCPUDescriptorHandleForHeapStart());
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

		retamMaterialCb.data.lineSize    = { 0.2f, 0.06f };
		retamMaterialCb.data.fading      = { 1.0f, 1.0f };
		retamMaterialCb.data.texScale    = { 0.3f, 0.3f, 0.3f, 0.3f };
		retamMaterialCb.data.crossAngle  = { 0.0f, 0.125f, 0.25f, 0.375f };
		retamMaterialCb.data.stripWidth  = 0.005f;
		retamMaterialCb.data.overdraw    = 1.0f;

		RunScript("scene.lua");

		GG_STRUCT(RetamMaterialCb)
			GG_MEMBER(lineSize)
			GG_MEMBER(fading)
			GG_MEMBER(texScale)
			GG_MEMBER(crossAngle)
			GG_MEMBER(stripWidth)
			GG_MEMBER(overdraw)
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

		auto reCollectIt = guiMaterials.find("retamReCollect");
		if (reCollectIt != guiMaterials.end()) {
			reCollectIt->second->SetConstantBuffer(retamMaterialCb);
			reCollectIt->second->SetSrvHeap(3, uavHeap, 0);
			reCollectIt->second->depthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		}

		auto depthIt = guiMaterials.find("layDownDepth");
		//if (depthIt != guiMaterials.end())
		//	depthIt->second->SetConstantBuffer(retamMaterialCb);

		SceneUploadResources();

		LoadTexture2D("textures/uvmask.png").CreateSRV(device.Get(), uavHeap.Get(), 10);
		LoadTexture2D("carrot.jpg").CreateSRV(device.Get(), uavHeap.Get(), 11);

		hipHop.createResources(device.Get(), (int)entities.size());
	}

	void recordComputeCommands() {
		auto& cmd = computeCommandLists[swapChainBackBufferIndex];
		ID3D12DescriptorHeap* pHeaps[] = { uavHeap.Get() };
		cmd->SetDescriptorHeaps(_countof(pHeaps), pHeaps);
		D3D12_GPU_DESCRIPTOR_HANDLE heap0 = uavHeap->GetGPUDescriptorHandleForHeapStart();

		strokeCountsBuffer.upload(cmd);
		strokeOffsetsBuffer.upload(cmd);

		//claudetest fragmentsBuffer.copyBack(cmd); //claudetest 
		//claudetest fragmentCountsBuffer.copyBack(cmd); //claudetest //xex

		sortCS.setup(cmd, heap0, 0);
		cmd->Dispatch(1024*16, 1, 1);
		cmd->ResourceBarrier(1, &strokeCountsBuffer.uavBarrier());

		//claudetest strokeCountsBuffer.copyBack(cmd); //claudetest 

		prefixSumCS.setup(cmd, heap0, 0);
		cmd->Dispatch(1, 1, 1);
		{
			D3D12_RESOURCE_BARRIER b[] = { strokeOffsetsBuffer.uavBarrier(), designBuffer.uavBarrier() };
			cmd->ResourceBarrier(2, b);
		}
		//claudetest designBuffer.copyBack(cmd); //claudetest 

		//claudetest strokeOffsetsBuffer.copyBack(cmd);

		compactCS.setup(cmd, heap0, 0);
		cmd->Dispatch(1024*16, 1, 1);
		
		argsCS.setup(cmd, heap0, 0);
		cmd->Dispatch(1, 1, 1);

		{
			D3D12_RESOURCE_BARRIER argsUAV = {};
			argsUAV.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			argsUAV.UAV.pResource = dispatchArgsResource.Get();
			D3D12_RESOURCE_BARRIER b[] = { strokeListBuffer.uavBarrier(), argsUAV };
			cmd->ResourceBarrier(2, b);
		}

		//claudetest strokeListBuffer.copyBack(cmd);

		{
			D3D12_RESOURCE_BARRIER toIndirect = {};
			toIndirect.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			toIndirect.Transition.pResource   = dispatchArgsResource.Get();
			toIndirect.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			toIndirect.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			toIndirect.Transition.StateAfter  = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
			cmd->ResourceBarrier(1, &toIndirect);
		}
		
		cubicCS.setup(cmd, heap0, 0);
		cmd->ExecuteIndirect(dispatchCommandSignature.Get(), 1, dispatchArgsResource.Get(), 0, nullptr, 0);

		{
			D3D12_RESOURCE_BARRIER fromIndirect = {};
			fromIndirect.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			fromIndirect.Transition.pResource   = dispatchArgsResource.Get();
			fromIndirect.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			fromIndirect.Transition.StateBefore = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
			fromIndirect.Transition.StateAfter  = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			cmd->ResourceBarrier(1, &fromIndirect);
		}
		{
			D3D12_RESOURCE_BARRIER b[] = { debugBuffer.uavBarrier(), cubicBuffer.uavBarrier() };
			cmd->ResourceBarrier(2, b);
		}
		//claudetest cubicBuffer.copyBack(cmd);
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

		if (false /*frameCount > 300*/) {
			// check compute results
			//uint* fragmentsData = (uint*)fragmentsBuffer.mapReadback(); //xex
			uint* fragmentCountsData = fragmentCountsBuffer.mapReadback();
			uint* strokeCountsData = strokeCountsBuffer.mapReadback();
			uint* strokeOffsetsData = strokeOffsetsBuffer.mapReadback();
			uint* strokeListData = strokeListBuffer.mapReadback();
			//uint* debugData = debugBuffer.mapReadback();
			float* cubicData = (float*)cubicBuffer.mapReadback();
			float* designData = (float*)designBuffer.mapReadback();

			/* claudetest
			if (frameCount == 400) {
				uint* fragmentsData      = (uint*)fragmentsBuffer.mapReadback();
				uint* fragmentCountsData = fragmentCountsBuffer.mapReadback();
				uint* strokeCountsData   = strokeCountsBuffer.mapReadback();
				float* designData        = (float*)designBuffer.mapReadback();

				constexpr uint pageSize         = 1024u;
				constexpr uint nPages           = 16u * 1024u;
				constexpr uint nStrokesPerPage  = 16u;
				constexpr uint designFloat4Size = 5u;
				constexpr uint designPagesInBuf = 16u * 5u * 1024u * 16u / (nStrokesPerPage * designFloat4Size);

				uint strokeMismatches = 0, designMismatches = 0;
				char buf[256];

				for (uint p = 0; p < nPages; p++) {
					uint nValid = std::min(fragmentCountsData[p], pageSize);
					if (nValid == 0) continue;

					const uint* page = fragmentsData + (size_t)p * pageSize * 4u;

					// Aggregate fragment count per unique stroke ID
					std::unordered_map<uint, uint> cpuCount;
					for (uint i = 0; i < nValid; i++)
						cpuCount[page[i * 4]]++;

					// Verify strokeCounts
					uint cpuUnique = (uint)cpuCount.size();
					uint gpuUnique = strokeCountsData[p];
					if (cpuUnique != gpuUnique) {
						snprintf(buf, sizeof(buf),
							"MISMATCH strokeCount p=%u cpu=%u gpu=%u nValid=%u\n",
							p, cpuUnique, gpuUnique, nValid);
						OutputDebugStringA(buf);
						strokeMismatches++;
					}

					// Verify design0.x (fragment count per stroke) against designBuffer
					// SIDs sorted ascending matches the GPU serial assignment order
					if (p < designPagesInBuf) {
						std::vector<uint> sortedSids;
						sortedSids.reserve(cpuCount.size());
						for (auto& kv : cpuCount) sortedSids.push_back(kv.first);
						std::sort(sortedSids.begin(), sortedSids.end());

						for (uint s = 0; s < (uint)sortedSids.size() && s < nStrokesPerPage; s++) {
							float cpuFragCount = (float)cpuCount[sortedSids[s]];
							// design0 is slot 2; x component = sum of 1 per fragment = fragment count
							uint d0xIdx = ((p * nStrokesPerPage + s) * designFloat4Size + 2u) * 4u;
							float gpuFragCount = designData[d0xIdx];
							if (fabsf(gpuFragCount - cpuFragCount) > 0.5f) {
								snprintf(buf, sizeof(buf),
									"MISMATCH design0.x p=%u s=%u sid=%u cpu=%.0f gpu=%.2f\n",
									p, s, sortedSids[s], cpuFragCount, gpuFragCount);
								OutputDebugStringA(buf);
								designMismatches++;
							}
						}
					}
				}

				snprintf(buf, sizeof(buf),
					"VERIFY: strokeMismatches=%u designMismatches=%u\n",
					strokeMismatches, designMismatches);
				OutputDebugStringA(buf);

				designBuffer.unmapReadback();
				strokeCountsBuffer.unmapReadback();
				fragmentCountsBuffer.unmapReadback();
				fragmentsBuffer.unmapReadback();
			}
			*/
/* {
				uint nValid = fragmentCountsData[142];
				uint* page = fragmentsBufferData + 142 * 1024 * 4;
				uint ids[1024];
				for (uint i = 0; i < nValid; i++)
					ids[i] = page[i * 4];
				std::sort(ids, ids + nValid);
				uint nUnique = 0;
				char dbgBuf[128];
				for (uint i = 0; i < nValid; ) {
					uint j = i + 1;
					while (j < nValid && ids[j] == ids[i]) j++;
					nUnique++;
					snprintf(dbgBuf, sizeof(dbgBuf), "  ID: %u  count: %u\n", ids[i], j - i);
					OutputDebugStringA(dbgBuf);
					i = j;
				}
				snprintf(dbgBuf, sizeof(dbgBuf), "Page 142: %u valid fragments, %u unique IDs\n", nValid, nUnique);
				OutputDebugStringA(dbgBuf);
			}
			*/
	/*		for (int si = 0; si < 1024 * 16; si++) {
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

			//for (int k = 0; k < 16382; k++) {
			//	if(fragmentCountsData[k] > 1024)
			//		bool kame = true;
			//}

			designBuffer.unmapReadback();
			cubicBuffer.unmapReadback();
			//debugBuffer.unmapReadback();
			strokeOffsetsData = strokeOffsetsBuffer.mapReadback();
			strokeListBuffer.unmapReadback();
			strokeCountsBuffer.unmapReadback();
			fragmentCountsBuffer.unmapReadback();
			//fragmentsBuffer.unmapReadback();
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
		if (showAnim) {
			hipHop.draw(commandList.Get(), 2, uavHeap.Get(),
				device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
				retamMaterialCb.GetGPUVirtualAddress(),
				perFrameCb.GetGPUVirtualAddress(),
				perObjectCb.GetGPUVirtualAddress());
		}
		else {
			for (int i = 0; i < (int)entities.size(); i++)
				entities[i]->Draw(commandList.Get(), 2, i);
		}

		// Collect pass — depth test via earlydepthstencil, writes to UAV buffers and debug RT
		CD3DX12_CPU_DESCRIPTOR_HANDLE collectRtvHandle(collectRtvHeap->GetCPUDescriptorHandleForHeapStart());
		const float collectClearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		commandList->ClearRenderTargetView(collectRtvHandle, collectClearColor, 0, nullptr);
		commandList->OMSetRenderTargets(1, &collectRtvHandle, FALSE, &collectDsvHandle);
		if (showStrokes) {
			if (showAnim) {
				hipHop.draw(commandList.Get(), 1, uavHeap.Get(),
					device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
					retamMaterialCb.GetGPUVirtualAddress(),
					perFrameCb.GetGPUVirtualAddress(),
					perObjectCb.GetGPUVirtualAddress());
			}
			else {
				for (int i = 0; i < (int)entities.size(); i++)
					entities[i]->Draw(commandList.Get(), 1, i);
			}
		}
		

		D3D12_RESOURCE_BARRIER b[] = { fragmentCountsBuffer.uavBarrier(), fragmentsBuffer.uavBarrier() };
		commandList->ResourceBarrier(2, b);

		//fragmentCountsBuffer.copyBack(commandList);
		//fragmentsBuffer.copyBack(commandList);
		//debugBuffer.copyBack(commandList);

		// Restore main viewport and render to swap chain
		commandList->RSSetViewports(1, &viewPort);
		commandList->RSSetScissorRects(1, &scissorRect);
		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[swapChainBackBufferIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
		commandList->OMSetRenderTargets(1, &rHandle, FALSE, &dsvHandle);

		const float clearColor[] = { 0.5f, 0.2f, 0.4f, 1.0f };
		commandList->ClearRenderTargetView(rHandle, clearColor, 0, nullptr);
		commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		if (showRetam) {
			if (showAnim) {
				hipHop.draw(commandList.Get(), 0, uavHeap.Get(),
					device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
					retamMaterialCb.GetGPUVirtualAddress(),
					perFrameCb.GetGPUVirtualAddress(),
					perObjectCb.GetGPUVirtualAddress());
			}
			else {
				__super::PopulateCommandList();
			}
		}

		{
			D3D12_RESOURCE_BARRIER barriers[] = {
				CD3DX12_RESOURCE_BARRIER::Transition(cubicBuffer.getResource(),
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
					D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
				CD3DX12_RESOURCE_BARRIER::Transition(dispatchArgsResource.Get(),
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT)
			};
			commandList->ResourceBarrier(2, barriers);
		}
		{
			uint dhIncrSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			CD3DX12_GPU_DESCRIPTOR_HANDLE cubicSrvGpu(uavHeap->GetGPUDescriptorHandleForHeapStart(), 9, dhIncrSize);
			ID3D12DescriptorHeap* heaps[] = { uavHeap.Get() };
			commandList->SetDescriptorHeaps(1, heaps);
			commandList->SetGraphicsRootSignature(cubicExtrudeRootSig.Get());
			commandList->SetGraphicsRootDescriptorTable(0, cubicSrvGpu);
			CD3DX12_GPU_DESCRIPTOR_HANDLE carrotSrvGpu(uavHeap->GetGPUDescriptorHandleForHeapStart(), 11, dhIncrSize);
			commandList->SetGraphicsRootDescriptorTable(1, carrotSrvGpu);
			commandList->SetGraphicsRootConstantBufferView(2, retamMaterialCb.GetGPUVirtualAddress());
			commandList->SetPipelineState(cubicExtrudePSO.Get());
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
			commandList->OMSetRenderTargets(1, &rHandle, FALSE, &dsvHandle);

			//const float clearColor[] = { 0.0f, 0.2f, 0.7f, 1.0f };
			//commandList->ClearRenderTargetView(rHandle, clearColor, 0, nullptr);
			//commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

			if (showReCollect) {
				if (showAnim) {
				}
				else {
					for (int i = 0; i < (int)entities.size(); i++)
						entities[i]->Draw(commandList.Get(), 3, i);
				}
			}
			
			if (showStrokes) {
				commandList->ExecuteIndirect(drawCommandSignature.Get(), 1, dispatchArgsResource.Get(), 12, nullptr, 0);
			}

		}
		{
			D3D12_RESOURCE_BARRIER barriers[] = {
				CD3DX12_RESOURCE_BARRIER::Transition(cubicBuffer.getResource(),
					D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
				CD3DX12_RESOURCE_BARRIER::Transition(dispatchArgsResource.Get(),
					D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
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
