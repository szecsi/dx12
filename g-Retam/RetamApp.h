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
#include "Egg/Compute/Fence.h"
#include "Egg/Shader.h"

#include "Shaders/Retam/RetamCb.hlsli"
#include "Egg/Script/StructReflectionMap.h"
#include "Egg/ConstantBuffer.hpp"
#include "StereoCamera.h"

#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

class RetamApp : public Egg::Script::ScriptedApp {

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

		auto addFloatSlider = [&](const std::wstring& label, float* ptr, float minVal, float maxVal) {
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
		};

		addFloatSlider(L"Eye Separation", &eyeSeparation, 0.0f, 0.5f);
		addFloatSlider(L"Focus Distance", &convergenceDistance, 0.1f, 20.0f);

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
				addFloatSlider(label, ptr, minVal, maxVal);
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
	uint64_t frameSyncValue = 1; // monotonically increasing value for uploadFence, reused as a hard per-stage GPU barrier within PopulateCommandList

	com_ptr<ID3D12Resource> collectDepthBuffer;
	com_ptr<ID3D12DescriptorHeap> collectDsvHeap;
	com_ptr<ID3D12Resource> collectColorBuffer;
	com_ptr<ID3D12DescriptorHeap> collectRtvHeap;

	// -- Stereo: each eye's fully-shaded retam render (the entire collect ->
	// compute -> stroke-extrude pipeline run once per eye, see
	// PopulateCommandList()), composited into a red/cyan anaglyph as the
	// final pass (RecordAndSubmitComposite()).
	float eyeSeparation = 0.065f;
	float convergenceDistance = 4.5f;
	com_ptr<ID3D12Resource> eyeColorBuffer[2]; // 0 = left, 1 = right
	D3D12_CPU_DESCRIPTOR_HANDLE eyeColorRtv[2]{};
	com_ptr<ID3D12DescriptorHeap> eyeColorRtvHeap;
	com_ptr<ID3D12RootSignature> anaglyphRootSig;
	com_ptr<ID3D12PipelineState> anaglyphPso;

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

		showRetam = true;
		showStrokes = true;
		showReCollect = false;

		uploadFence.createResources(device);

		D3D12_DESCRIPTOR_HEAP_DESC dhd;
		dhd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		dhd.NodeMask = 0;
		dhd.NumDescriptors = 14;
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
			psoDesc.DepthStencilState.DepthEnable = FALSE;
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

		{
			com_ptr<ID3DBlob> vs = Egg::Shader::LoadCso("Shaders/Retam/retamAnaglyphVS.cso");
			com_ptr<ID3DBlob> ps = Egg::Shader::LoadCso("Shaders/Retam/retamAnaglyphPS.cso");
			anaglyphRootSig = Egg::Shader::LoadRootSignature(device.Get(), vs.Get());

			D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
			psoDesc.pRootSignature = anaglyphRootSig.Get();
			psoDesc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
			psoDesc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
			psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
			psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			psoDesc.DepthStencilState.DepthEnable = FALSE;
			psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			D3D12_INPUT_LAYOUT_DESC emptyLayout = { nullptr, 0 };
			psoDesc.InputLayout = emptyLayout;
			psoDesc.NumRenderTargets = 1;
			psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
			psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
			psoDesc.SampleMask = UINT_MAX;
			psoDesc.SampleDesc.Count = 1;
			DX_API("create anaglyph composite PSO")
				device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(anaglyphPso.GetAddressOf()));
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

		D3D12_DESCRIPTOR_HEAP_DESC eyeColorRtvHeapDesc = {};
		eyeColorRtvHeapDesc.NumDescriptors = 2;
		eyeColorRtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		eyeColorRtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		DX_API("create eye color RTV heap")
			device->CreateDescriptorHeap(&eyeColorRtvHeapDesc, IID_PPV_ARGS(eyeColorRtvHeap.ReleaseAndGetAddressOf()));
		uint eyeRtvIncrSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		uint eyeCbvSrvUavIncrSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		UINT ew = (UINT)viewPort.Width, eh = (UINT)viewPort.Height;
		const wchar_t* eyeNames[2] = { L"EyeColorL", L"EyeColorR" };
		for (int e = 0; e < 2; e++) {
			D3D12_CLEAR_VALUE eyeColorClearVal = {};
			eyeColorClearVal.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			eyeColorClearVal.Color[0] = eyeColorClearVal.Color[1] = eyeColorClearVal.Color[2] = eyeColorClearVal.Color[3] = 1.0f;
			DX_API("create eye color buffer")
				device->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
					D3D12_HEAP_FLAG_NONE,
					&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, ew, eh, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),
					D3D12_RESOURCE_STATE_RENDER_TARGET,
					&eyeColorClearVal,
					IID_PPV_ARGS(eyeColorBuffer[e].ReleaseAndGetAddressOf()));
			eyeColorBuffer[e]->SetName(eyeNames[e]);

			eyeColorRtv[e] = CD3DX12_CPU_DESCRIPTOR_HANDLE(eyeColorRtvHeap->GetCPUDescriptorHandleForHeapStart(), e, eyeRtvIncrSize);
			device->CreateRenderTargetView(eyeColorBuffer[e].Get(), nullptr, eyeColorRtv[e]);

			D3D12_SHADER_RESOURCE_VIEW_DESC eyeColorSrvDesc = {};
			eyeColorSrvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			eyeColorSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			eyeColorSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			eyeColorSrvDesc.Texture2D.MipLevels = 1;
			CD3DX12_CPU_DESCRIPTOR_HANDLE eyeColorSrvHandle(uavHeap->GetCPUDescriptorHandleForHeapStart(), 12 + e, eyeCbvSrvUavIncrSize);
			device->CreateShaderResourceView(eyeColorBuffer[e].Get(), &eyeColorSrvDesc, eyeColorSrvHandle);
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

		LoadTexture2D("textures/uvmask.png").CreateSRV(device.Get(), uavHeap.Get(), 10);
		LoadTexture2D("gayline.png").CreateSRV(device.Get(), uavHeap.Get(), 11);

		SceneUploadResources();

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

	// Forces the GPU to fully catch up before the CPU proceeds. Needed
	// between the stages below because, unlike the rest of this codebase's
	// per-swapchain-frame double buffering, the stereo eye loop reuses the
	// SAME single-buffered collect/UAV/cubic-stroke resources twice within
	// one real frame (once per eye) -- the next stage may not start
	// recording into them until the GPU has actually finished the previous
	// one.
	void HardSync(com_ptr<ID3D12CommandQueue> queue) {
		frameSyncValue++;
		uploadFence.signal(queue, frameSyncValue);
		uploadFence.cpuWait();
	}

	// Derives eye's off-axis view/projection from the single head camera
	// (see StereoCamera.h) and pushes it into perFrameCb -- every shader in
	// the retam pipeline (collect, base shading, cubic stroke extrude) reads
	// its transform from there, so this alone is enough to steer the whole
	// process at one eye.
	void UpdateEyeCamera(int eye) {
		using namespace Egg::Math;
		auto headCam = std::dynamic_pointer_cast<Egg::Cam::FirstPerson>(cameras[currentCameraIndex]);
		if (!headCam) return;

		bool rightEye = (eye == 1);
		float4x4 view = Stereo::ComputeEyeView(headCam, eyeSeparation, rightEye);
		float4x4 proj = Stereo::ComputeEyeProj(headCam->GetFov(), headCam->GetAspect(), headCam->GetNearPlane(), headCam->GetFarPlane(),
			eyeSeparation, convergenceDistance, rightEye);
		float3 eyePos = headCam->GetEyePosition() + Stereo::HeadRight(headCam) * (rightEye ? 0.5f : -0.5f) * eyeSeparation;

		perFrameCb->viewProjTransform = view * proj;
		perFrameCb->cameraPos = float4(eyePos, 1.0f);
		perFrameCb->ahead = float4(headCam->GetAhead(), 0.0f);
		perFrameCb.Upload();
	}

	// Lays down depth and collects fragments (UV/curvature seeds for the
	// strokes) into the 1024x1024 offscreen collect targets, for whichever
	// eye's transform is currently in perFrameCb. Self-contained: submitted
	// and hard-synced before returning, so the compute pass that consumes
	// fragmentCountsBuffer/fragmentsBuffer right after is guaranteed to see
	// THIS eye's fragments, not a stale or in-flight write.
	void RecordAndSubmitCollectPass() {
		DX_API("Failed to reset command allocator (Collect)")
			commandAllocator->Reset();
		DX_API("Failed to reset command list (Collect)")
			commandList->Reset(commandAllocator.Get(), nullptr);

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

		DX_API("close collect command list")
			commandList->Close();
		ID3D12CommandList* lists[] = { commandList.Get() };
		commandQueue->ExecuteCommandLists(1, lists);
		HardSync(commandQueue);
	}

	// Sort/prefix-sum/compact/extract strokes from the fragments the collect
	// pass just wrote. Self-contained (submitted + hard-synced) so the final
	// draw pass right after sees THIS eye's finished cubicBuffer/strokeList,
	// not a partially-written one.
	void RecordAndSubmitComputePass() {
		auto& cmd = computeCommandLists[swapChainBackBufferIndex];
		DX_API("Failed to reset compute command allocator")
			computeAllocators[swapChainBackBufferIndex]->Reset();
		DX_API("Failed to reset compute command list")
			cmd->Reset(computeAllocators[swapChainBackBufferIndex].Get(), nullptr);

		recordComputeCommands();

		DX_API("close compute command list")
			cmd->Close();
		ID3D12CommandList* lists[] = { cmd.Get() };
		computeCommandQueue->ExecuteCommandLists(1, lists);
		HardSync(computeCommandQueue);
	}

	// Draws this eye's fully-shaded retam image (base pass + cubic-extruded
	// strokes) into eyeColorBuffer[eye], cleared to white paper first. Left
	// in an SRV-readable state on return, ready for RecordAndSubmitComposite().
	void RecordAndSubmitFinalDrawPass(int eye) {
		DX_API("Failed to reset command allocator (FinalDraw)")
			commandAllocator->Reset();
		DX_API("Failed to reset command list (FinalDraw)")
			commandList->Reset(commandAllocator.Get(), nullptr);

		CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(dsvHeap->GetCPUDescriptorHandleForHeapStart());

		commandList->RSSetViewports(1, &viewPort);
		commandList->RSSetScissorRects(1, &scissorRect);
		commandList->OMSetRenderTargets(1, &eyeColorRtv[eye], FALSE, &dsvHandle);

		const float whiteClear[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		commandList->ClearRenderTargetView(eyeColorRtv[eye], whiteClear, 0, nullptr);
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
			commandList->OMSetRenderTargets(1, &eyeColorRtv[eye], FALSE, &dsvHandle);

			if (showReCollect) {
				if (!showAnim) {
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

		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			eyeColorBuffer[eye].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

		DX_API("close final draw command list")
			commandList->Close();
		ID3D12CommandList* lists[] = { commandList.Get() };
		commandQueue->ExecuteCommandLists(1, lists);
		HardSync(commandQueue);
	}

	// Pass D: red/cyan anaglyph combine of both eyes' finished renders into
	// the swap chain backbuffer (cleared white — see retamAnaglyphPS.hlsl).
	// Left open (closed but not executed) for the base App::Render() call
	// that follows PopulateCommandList() to execute and present.
	void RecordAndSubmitComposite() {
		DX_API("Failed to reset command allocator (Composite)")
			commandAllocator->Reset();
		DX_API("Failed to reset command list (Composite)")
			commandList->Reset(commandAllocator.Get(), nullptr);

		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			renderTargets[swapChainBackBufferIndex].Get(),
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

		CD3DX12_CPU_DESCRIPTOR_HANDLE rHandle(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), swapChainBackBufferIndex, rtvDescriptorHandleIncrementSize);
		commandList->OMSetRenderTargets(1, &rHandle, FALSE, nullptr);
		commandList->RSSetViewports(1, &viewPort);
		commandList->RSSetScissorRects(1, &scissorRect);
		const float whiteBg[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		commandList->ClearRenderTargetView(rHandle, whiteBg, 0, nullptr);

		uint dhIncrSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		CD3DX12_GPU_DESCRIPTOR_HANDLE eyeColorSrvGpu(uavHeap->GetGPUDescriptorHandleForHeapStart(), 12, dhIncrSize);
		ID3D12DescriptorHeap* heaps[] = { uavHeap.Get() };
		commandList->SetDescriptorHeaps(1, heaps);
		commandList->SetGraphicsRootSignature(anaglyphRootSig.Get());
		commandList->SetGraphicsRootDescriptorTable(0, eyeColorSrvGpu);
		commandList->SetPipelineState(anaglyphPso.Get());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->DrawInstanced(3, 1, 0, 0);

		D3D12_RESOURCE_BARRIER backToRt[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(eyeColorBuffer[0].Get(),
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_RENDER_TARGET),
			CD3DX12_RESOURCE_BARRIER::Transition(eyeColorBuffer[1].Get(),
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_RENDER_TARGET),
		};
		commandList->ResourceBarrier(2, backToRt);

		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			renderTargets[swapChainBackBufferIndex].Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

		DX_API("close graphics command list")
			commandList->Close();
	}

	// The entire retam process (collect -> compute -> stroke extrude) run
	// once per eye into its own offscreen color target, then combined into
	// a red/cyan anaglyph. Each stage is hard-synced before the next
	// begins (see HardSync()), since both eyes share the same single-
	// buffered collect/UAV/cubic-stroke resources.
	virtual void PopulateCommandList() override {
		for (int eye = 0; eye < 2; eye++) {
			UpdateEyeCamera(eye);
			RecordAndSubmitCollectPass();
			RecordAndSubmitComputePass();
			RecordAndSubmitFinalDrawPass(eye);
		}
		RecordAndSubmitComposite();
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
		eyeColorBuffer[0].Reset();
		eyeColorBuffer[1].Reset();
		eyeColorRtvHeap.Reset();
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
		anaglyphPso.Reset();
		anaglyphRootSig.Reset();
		uavHeap.Reset();
		Egg::Script::ScriptedApp::ReleaseResources();
	}
};
