#pragma once
#include "Egg/Common.h"
#include <Egg/Script/ScriptedApp.h>
#include <algorithm>
#include <unordered_map>

#include "Egg/Compute/RawBuffer.h"
#include "Egg/Compute/TypedBuffer.h"
#include "Egg/Compute/ComputePass.h"
#include "Egg/Compute/FenceChain.h"
#include "Egg/Shader.h"
#include "Egg/Importer.h"

#include "Shaders/TreeCB.hlsli"
#include "Egg/Script/StructReflectionMap.h"
#include "Egg/ConstantBuffer.hpp"

#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

#include <DirectXTex/DirectXTex.h>
#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")

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
	* 124k pieces per single instance set of models
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
	static const unsigned int nMinPiecesPerTree = 33;
	static const unsigned int nMaxPiecesPerTree = 127;
	static const unsigned int nAveragePieces = (nMinPiecesPerTree + nMaxPiecesPerTree) / 2;

	com_ptr<ID3D12DescriptorHeap> uavHeap;
	Egg::Compute::RawBuffer   instanceBuffers[nLevels * nBranchTypes];
	Egg::Compute::TypedBuffer bonesBuffer;
	Egg::Compute::RawBuffer   twistsBuffer;

	com_ptr<ID3D12Resource>          dispatchArgsResource;
	com_ptr<ID3D12CommandSignature>  dispatchCommandSignature;

	Egg::Compute::ComputeShader growCS;
	//Egg::Compute::ComputeShader treeherdCS;

	com_ptr<ID3D12CommandSignature>  drawCommandSignature;

	// Billboard bake: a single tree (model 0), rendered from several angles
	// into a horizontal texture atlas, for use as a far-away tree impostor.
	static constexpr unsigned int billboardTileSize  = 512;
	static constexpr unsigned int billboardViewCount = 8;
	// world-space width/height of the baked tile == of the billboard quad; must match the
	// literal of the same name (and purpose) in Shaders/billboardTreeVS.hlsl
	static constexpr float billboardWorldSize = 16.0f;
	static constexpr unsigned int billboardInstanceCount = 3000;

	com_ptr<ID3D12Resource>       billboardAtlas;
	com_ptr<ID3D12DescriptorHeap> billboardAtlasSrvHeap;

	com_ptr<ID3D12Resource>       billboardPositionsBuffer;
	com_ptr<ID3D12DescriptorHeap> billboardRenderSrvHeap;
	com_ptr<ID3D12RootSignature>  billboardRenderRootSig;
	com_ptr<ID3D12PipelineState>  billboardRenderPso;

	static Egg::Math::float4x4 OrthoLH(float w, float h, float zn, float zf) {
		using namespace Egg::Math;
		float4x4 m = float4x4::Identity;
		m._00 = 2.0f / w;
		m._11 = 2.0f / h;
		m._22 = 1.0f / (zf - zn);
		m._32 = -zn / (zf - zn);
		return m;
	}

	// Renders tree model 0 from billboardViewCount angles into billboardAtlas
	// and saves the result to Media/tree/billboard_atlas.png. Runs once, synchronously,
	// after the scene (and its compute buffers) have been uploaded.
	void BakeBillboardTexture() {
		using namespace Egg::Math;

		treeMaterialCb.Upload();

		Egg::Mesh::IndexedGeometry::P bakeGeometry = std::dynamic_pointer_cast<Egg::Mesh::IndexedGeometry>(
			Egg::Importer::ImportWithTangentSpaceAndRigging(device.Get(), "tree/y0.fbx"));
		bakeGeometry->instanceCount = 256;

		com_ptr<ID3DBlob> bakeVSBlob = Egg::Shader::LoadCso("Shaders/billboardBakeVS.cso");
		com_ptr<ID3D12RootSignature> bakeRootSig = Egg::Shader::LoadRootSignature(device.Get(), bakeVSBlob.Get());
		com_ptr<ID3DBlob> bakePSBlob = Egg::Shader::LoadCso("Shaders/treePS.cso");

		uint dhIncrSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		com_ptr<ID3D12DescriptorHeap> bakeSrvHeap;
		D3D12_DESCRIPTOR_HEAP_DESC bakeSrvHeapDesc;
		bakeSrvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		bakeSrvHeapDesc.NodeMask = 0;
		bakeSrvHeapDesc.NumDescriptors = 3;
		bakeSrvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		DX_API("create billboard bake srv heap")
			device->CreateDescriptorHeap(&bakeSrvHeapDesc, IID_PPV_ARGS(bakeSrvHeap.GetAddressOf()));
		instanceBuffers[0].createSrv(device, CD3DX12_CPU_DESCRIPTOR_HANDLE(bakeSrvHeap->GetCPUDescriptorHandleForHeapStart(), 0, dhIncrSize));
		bonesBuffer.createSrv(device, CD3DX12_CPU_DESCRIPTOR_HANDLE(bakeSrvHeap->GetCPUDescriptorHandleForHeapStart(), 1, dhIncrSize));
		twistsBuffer.createSrv(device, CD3DX12_CPU_DESCRIPTOR_HANDLE(bakeSrvHeap->GetCPUDescriptorHandleForHeapStart(), 2, dhIncrSize));

		const unsigned int atlasWidth  = billboardTileSize * billboardViewCount;
		const unsigned int atlasHeight = billboardTileSize;

		D3D12_RESOURCE_DESC atlasDesc = {};
		atlasDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		atlasDesc.Width = atlasWidth;
		atlasDesc.Height = atlasHeight;
		atlasDesc.DepthOrArraySize = 1;
		atlasDesc.MipLevels = 1;
		atlasDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		atlasDesc.SampleDesc.Count = 1;
		atlasDesc.SampleDesc.Quality = 0;
		atlasDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		atlasDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		D3D12_CLEAR_VALUE atlasClear;
		atlasClear.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		atlasClear.Color[0] = atlasClear.Color[1] = atlasClear.Color[2] = atlasClear.Color[3] = 0.0f;

		DX_API("create billboard atlas texture")
			device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&atlasDesc,
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				&atlasClear,
				IID_PPV_ARGS(billboardAtlas.ReleaseAndGetAddressOf()));
		billboardAtlas->SetName(L"BillboardAtlas");

		D3D12_DESCRIPTOR_HEAP_DESC atlasSrvHeapDesc;
		atlasSrvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		atlasSrvHeapDesc.NodeMask = 0;
		atlasSrvHeapDesc.NumDescriptors = 1;
		atlasSrvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		DX_API("create billboard atlas srv heap")
			device->CreateDescriptorHeap(&atlasSrvHeapDesc, IID_PPV_ARGS(billboardAtlasSrvHeap.ReleaseAndGetAddressOf()));

		D3D12_SHADER_RESOURCE_VIEW_DESC atlasSrvDesc = {};
		atlasSrvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		atlasSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		atlasSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		atlasSrvDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(billboardAtlas.Get(), &atlasSrvDesc, billboardAtlasSrvHeap->GetCPUDescriptorHandleForHeapStart());

		D3D12_RESOURCE_DESC depthDesc = atlasDesc;
		depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
		depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE depthClear;
		depthClear.Format = DXGI_FORMAT_D32_FLOAT;
		depthClear.DepthStencil.Depth = 1.0f;
		depthClear.DepthStencil.Stencil = 0;

		com_ptr<ID3D12Resource> bakeDepth;
		DX_API("create billboard bake depth buffer")
			device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&depthDesc,
				D3D12_RESOURCE_STATE_DEPTH_WRITE,
				&depthClear,
				IID_PPV_ARGS(bakeDepth.GetAddressOf()));

		com_ptr<ID3D12DescriptorHeap> bakeRtvHeap, bakeDsvHeap;
		D3D12_DESCRIPTOR_HEAP_DESC bakeRtvHeapDesc;
		bakeRtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		bakeRtvHeapDesc.NodeMask = 0;
		bakeRtvHeapDesc.NumDescriptors = 1;
		bakeRtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		DX_API("create billboard bake rtv heap")
			device->CreateDescriptorHeap(&bakeRtvHeapDesc, IID_PPV_ARGS(bakeRtvHeap.GetAddressOf()));
		device->CreateRenderTargetView(billboardAtlas.Get(), nullptr, bakeRtvHeap->GetCPUDescriptorHandleForHeapStart());

		D3D12_DESCRIPTOR_HEAP_DESC bakeDsvHeapDesc;
		bakeDsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		bakeDsvHeapDesc.NodeMask = 0;
		bakeDsvHeapDesc.NumDescriptors = 1;
		bakeDsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		DX_API("create billboard bake dsv heap")
			device->CreateDescriptorHeap(&bakeDsvHeapDesc, IID_PPV_ARGS(bakeDsvHeap.GetAddressOf()));
		device->CreateDepthStencilView(bakeDepth.Get(), nullptr, bakeDsvHeap->GetCPUDescriptorHandleForHeapStart());

		D3D12_GRAPHICS_PIPELINE_STATE_DESC bakePsoDesc = {};
		bakePsoDesc.pRootSignature = bakeRootSig.Get();
		bakePsoDesc.VS = CD3DX12_SHADER_BYTECODE(bakeVSBlob.Get());
		bakePsoDesc.PS = CD3DX12_SHADER_BYTECODE(bakePSBlob.Get());
		bakePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		bakePsoDesc.SampleMask = UINT_MAX;
		bakePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		bakePsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		bakePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		bakePsoDesc.InputLayout = bakeGeometry->GetInputLayout();
		bakePsoDesc.PrimitiveTopologyType = bakeGeometry->GetTopologyType();
		bakePsoDesc.NumRenderTargets = 1;
		bakePsoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		bakePsoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		bakePsoDesc.SampleDesc.Count = 1;

		com_ptr<ID3D12PipelineState> bakePso;
		DX_API("create billboard bake pso")
			device->CreateGraphicsPipelineState(&bakePsoDesc, IID_PPV_ARGS(bakePso.GetAddressOf()));

		// object transform (identity: the bake VS already places the tree at the origin)
		__declspec(align(16)) struct BakeObjectCb {
			float4x4 modelMat;
			float4x4 modelMatInv;
		};
		Egg::ConstantBuffer<BakeObjectCb> objCb;
		objCb.CreateResources(device.Get());
		objCb.data.modelMat = float4x4::Identity;
		objCb.data.modelMatInv = float4x4::Identity;
		objCb.Upload();

		// one constant buffer slice per camera angle
		const UINT viewCbStride = 256;
		com_ptr<ID3D12Resource> viewProjCb;
		DX_API("create billboard bake view-proj cb")
			device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(viewCbStride * billboardViewCount),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(viewProjCb.GetAddressOf()));

		const float treeCenterHeight = billboardWorldSize * 0.5f;
		const float orthoExtent      = billboardWorldSize;
		const float camDistance      = 40.0f;
		const float nearZ = 0.1f, farZ = 100.0f;
		const float3 center(0.0f, treeCenterHeight, 0.0f);

		{
			UINT8* mappedViewCb = nullptr;
			CD3DX12_RANGE noRead(0, 0);
			DX_API("map billboard bake view-proj cb")
				viewProjCb->Map(0, &noRead, reinterpret_cast<void**>(&mappedViewCb));
			for (unsigned int i = 0; i < billboardViewCount; i++) {
				float theta = (2.0f * 3.14159265358979f * i) / billboardViewCount;
				float3 eye = center + float3(sinf(theta), 0.0f, cosf(theta)) * camDistance;
				float3 ahead = center - eye;
				float4x4 view = float4x4::View(eye, ahead, float3(0.0f, 1.0f, 0.0f));
				float4x4 proj = OrthoLH(orthoExtent, orthoExtent, nearZ, farZ);
				float4x4 viewProj = view * proj;
				memcpy(mappedViewCb + i * viewCbStride, &viewProj, sizeof(float4x4));
			}
			viewProjCb->Unmap(0, nullptr);
		}
		D3D12_GPU_VIRTUAL_ADDRESS viewCbBase = viewProjCb->GetGPUVirtualAddress();

		commandAllocator->Reset();
		commandList->Reset(commandAllocator.Get(), nullptr);

		{
			ID3D12DescriptorHeap* growHeaps[] = { uavHeap.Get() };
			commandList->SetDescriptorHeaps(_countof(growHeaps), growHeaps);
			growCS.setup(commandList, uavHeap->GetGPUDescriptorHandleForHeapStart(), 0);
			commandList->SetComputeRootConstantBufferView(0, treeMaterialCb.GetGPUVirtualAddress());
			commandList->Dispatch(nTreeModels, 1, 1);
			D3D12_RESOURCE_BARRIER growBarriers[] = {
				instanceBuffers[0].uavBarrier(),
				bonesBuffer.uavBarrier(),
				twistsBuffer.uavBarrier()
			};
			commandList->ResourceBarrier(_countof(growBarriers), growBarriers);
		}

		D3D12_RESOURCE_BARRIER toSrv[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(instanceBuffers[0].getResource(),
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(bonesBuffer.getResource(),
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(twistsBuffer.getResource(),
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
		};
		commandList->ResourceBarrier(_countof(toSrv), toSrv);

		CD3DX12_CPU_DESCRIPTOR_HANDLE bakeRtvHandle(bakeRtvHeap->GetCPUDescriptorHandleForHeapStart());
		CD3DX12_CPU_DESCRIPTOR_HANDLE bakeDsvHandle(bakeDsvHeap->GetCPUDescriptorHandleForHeapStart());
		commandList->OMSetRenderTargets(1, &bakeRtvHandle, FALSE, &bakeDsvHandle);
		const float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		commandList->ClearRenderTargetView(bakeRtvHandle, clearColor, 0, nullptr);
		commandList->ClearDepthStencilView(bakeDsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		ID3D12DescriptorHeap* bakeHeaps[] = { bakeSrvHeap.Get() };
		commandList->SetDescriptorHeaps(_countof(bakeHeaps), bakeHeaps);
		commandList->SetGraphicsRootSignature(bakeRootSig.Get());
		commandList->SetPipelineState(bakePso.Get());
		commandList->SetGraphicsRootConstantBufferView(0, treeMaterialCb.GetGPUVirtualAddress());
		commandList->SetGraphicsRootConstantBufferView(2, objCb.GetGPUVirtualAddress());
		commandList->SetGraphicsRootDescriptorTable(3, bakeSrvHeap->GetGPUDescriptorHandleForHeapStart());

		for (unsigned int i = 0; i < billboardViewCount; i++) {
			D3D12_VIEWPORT vp{ (float)(i * billboardTileSize), 0.0f, (float)billboardTileSize, (float)billboardTileSize, 0.0f, 1.0f };
			D3D12_RECT sc{ (LONG)(i * billboardTileSize), 0, (LONG)((i + 1) * billboardTileSize), (LONG)billboardTileSize };
			commandList->RSSetViewports(1, &vp);
			commandList->RSSetScissorRects(1, &sc);
			commandList->SetGraphicsRootConstantBufferView(1, viewCbBase + i * viewCbStride);
			bakeGeometry->Draw(commandList.Get());
		}

		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(billboardAtlas.Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE));

		D3D12_RESOURCE_BARRIER backToUav[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(instanceBuffers[0].getResource(),
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
			CD3DX12_RESOURCE_BARRIER::Transition(bonesBuffer.getResource(),
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
			CD3DX12_RESOURCE_BARRIER::Transition(twistsBuffer.getResource(),
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
		};
		commandList->ResourceBarrier(_countof(backToUav), backToUav);

		const UINT64 rowPitch = ((UINT64)atlasWidth * 4u + 255u) & ~(UINT64)255u;
		const UINT64 readbackSize = rowPitch * atlasHeight;

		com_ptr<ID3D12Resource> atlasReadback;
		DX_API("create billboard atlas readback buffer")
			device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(readbackSize),
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(atlasReadback.GetAddressOf()));

		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
		footprint.Offset = 0;
		footprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		footprint.Footprint.Width = atlasWidth;
		footprint.Footprint.Height = atlasHeight;
		footprint.Footprint.Depth = 1;
		footprint.Footprint.RowPitch = (UINT)rowPitch;

		CD3DX12_TEXTURE_COPY_LOCATION copyDst(atlasReadback.Get(), footprint);
		CD3DX12_TEXTURE_COPY_LOCATION copySrc(billboardAtlas.Get(), 0);
		commandList->CopyTextureRegion(&copyDst, 0, 0, 0, &copySrc, nullptr);

		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(billboardAtlas.Get(),
			D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

		DX_API("close billboard bake command list")
			commandList->Close();

		ID3D12CommandList* bakeLists[] = { commandList.Get() };
		commandQueue->ExecuteCommandLists(1, bakeLists);

		WaitForPreviousFrame();

		UINT8* mappedReadback = nullptr;
		CD3DX12_RANGE readRange(0, (SIZE_T)readbackSize);
		DX_API("map billboard atlas readback buffer")
			atlasReadback->Map(0, &readRange, reinterpret_cast<void**>(&mappedReadback));

		DirectX::Image img = {};
		img.width = atlasWidth;
		img.height = atlasHeight;
		img.format = DXGI_FORMAT_R8G8B8A8_UNORM;
		img.rowPitch = (size_t)rowPitch;
		img.slicePitch = (size_t)readbackSize;
		img.pixels = mappedReadback;

		HRESULT coHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
		bool comInitializedHere = (coHr == S_OK || coHr == S_FALSE);

		DX_API("save billboard atlas to disk")
			DirectX::SaveToWICFile(img, DirectX::WIC_FLAGS_NONE, GUID_ContainerFormatPng, L"../Media/tree/billboard_atlas.png");

		if (comInitializedHere) CoUninitialize();

		atlasReadback->Unmap(0, nullptr);
	}

	// Scatters billboardInstanceCount cheap billboard trees in an annulus surrounding
	// the detailed animated forest (which spans roughly [-500,500] in x/z), and builds
	// the PSO/SRVs needed to draw them from billboardAtlas every frame. Must run after
	// BakeBillboardTexture() (billboardAtlas must already exist).
	void CreateBillboardInstances() {
		const float nearRadius = 600.0f;
		const float farRadius  = 4000.0f;

		DX_API("create billboard positions buffer")
			device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(sizeof(float) * 4 * billboardInstanceCount),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(billboardPositionsBuffer.ReleaseAndGetAddressOf()));

		{
			float* mapped = nullptr;
			CD3DX12_RANGE noRead(0, 0);
			DX_API("map billboard positions buffer")
				billboardPositionsBuffer->Map(0, &noRead, reinterpret_cast<void**>(&mapped));
			for (unsigned int i = 0; i < billboardInstanceCount; i++) {
				float haltonR = 0.0f, haltonA = 0.0f, haltonS = 0.0f;
				unsigned int hn = i + 1u; float hf = 1.0f;
				while (hn > 0u) { hf *= 0.5f; haltonR += (hn & 1u) ? hf : 0.0f; hn >>= 1u; }
				hn = i + 1u; hf = 1.0f;
				while (hn > 0u) { hf /= 3.0f; haltonA += (hn % 3u) * hf; hn /= 3u; }
				hn = i + 1u; hf = 1.0f;
				while (hn > 0u) { hf /= 5.0f; haltonS += (hn % 5u) * hf; hn /= 5u; }

				float radius = nearRadius + haltonR * (farRadius - nearRadius);
				float angle  = haltonA * 6.28318530718f;
				float scale  = 0.8f + 0.6f * haltonS;

				mapped[i * 4 + 0] = radius * sinf(angle);
				mapped[i * 4 + 1] = 0.0f;
				mapped[i * 4 + 2] = radius * cosf(angle);
				mapped[i * 4 + 3] = scale;
			}
			billboardPositionsBuffer->Unmap(0, nullptr);
		}

		uint dhIncrSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc;
		srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		srvHeapDesc.NodeMask = 0;
		srvHeapDesc.NumDescriptors = 2;
		srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		DX_API("create billboard render srv heap")
			device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(billboardRenderSrvHeap.ReleaseAndGetAddressOf()));

		D3D12_SHADER_RESOURCE_VIEW_DESC posSrvDesc = {};
		posSrvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		posSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		posSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		posSrvDesc.Buffer.NumElements = billboardInstanceCount;
		device->CreateShaderResourceView(billboardPositionsBuffer.Get(), &posSrvDesc,
			CD3DX12_CPU_DESCRIPTOR_HANDLE(billboardRenderSrvHeap->GetCPUDescriptorHandleForHeapStart(), 0, dhIncrSize));

		D3D12_SHADER_RESOURCE_VIEW_DESC atlasSrvDesc = {};
		atlasSrvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		atlasSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		atlasSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		atlasSrvDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(billboardAtlas.Get(), &atlasSrvDesc,
			CD3DX12_CPU_DESCRIPTOR_HANDLE(billboardRenderSrvHeap->GetCPUDescriptorHandleForHeapStart(), 1, dhIncrSize));

		com_ptr<ID3DBlob> billboardVSBlob = Egg::Shader::LoadCso("Shaders/billboardTreeVS.cso");
		billboardRenderRootSig = Egg::Shader::LoadRootSignature(device.Get(), billboardVSBlob.Get());
		com_ptr<ID3DBlob> billboardPSBlob = Egg::Shader::LoadCso("Shaders/billboardTreePS.cso");

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = billboardRenderRootSig.Get();
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(billboardVSBlob.Get());
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(billboardPSBlob.Get());
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.BlendState.AlphaToCoverageEnable = TRUE;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		D3D12_INPUT_LAYOUT_DESC emptyLayout = { nullptr, 0 };
		psoDesc.InputLayout = emptyLayout;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		psoDesc.SampleDesc.Count = 1;

		DX_API("create billboard render pso")
			device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(billboardRenderPso.ReleaseAndGetAddressOf()));
	}

	// Draws all billboard-tree instances into whatever render target/viewport is
	// currently bound. Must be called from within PopulateCommandList(), after the
	// main scene draw (so the RTV/DSV/viewport TreeApp set up earlier are still active).
	void RecordBillboardDraw() {
		commandList->SetGraphicsRootSignature(billboardRenderRootSig.Get());
		commandList->SetPipelineState(billboardRenderPso.Get());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		commandList->SetGraphicsRootConstantBufferView(0, perFrameCb.GetGPUVirtualAddress());
		ID3D12DescriptorHeap* heaps[] = { billboardRenderSrvHeap.Get() };
		commandList->SetDescriptorHeaps(_countof(heaps), heaps);
		commandList->SetGraphicsRootDescriptorTable(1, billboardRenderSrvHeap->GetGPUDescriptorHandleForHeapStart());
		commandList->DrawInstanced(4, billboardInstanceCount, 0, 0);
	}

public:
	TreeApp() : ScriptedApp(),
		instanceBuffers {
			Egg::Compute::RawBuffer(L"y2",
				nTreeModels * 256 * 4u /*TODO: 4 for psi case?, not needed here*/)
		},
		bonesBuffer(L"bones", nTreeModels * 256 * 2u/*nChildren*/ * 4u/*rows*/, DXGI_FORMAT_R32G32B32A32_FLOAT),
		twistsBuffer(L"twists", nTreeModels * 256)
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
		CD3DX12_CPU_DESCRIPTOR_HANDLE twistsHandle(uavHeap->GetCPUDescriptorHandleForHeapStart(), 2, dhIncrSize);
		twistsBuffer.createResources(device, twistsHandle);

		growCS.createResources(device, "Shaders/growCS.cso");
		//treeherdCS.createResources(device, "Shaders/treeherdCS.cso");

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

	void LuaAddTreeBuffers() {
		uint dhIncrSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		instanceBuffers[0].createSrv(device, CD3DX12_CPU_DESCRIPTOR_HANDLE(srvHeap->GetCPUDescriptorHandleForHeapStart(), srvCount, dhIncrSize));
		srvCount++;
		bonesBuffer.createSrv(device, CD3DX12_CPU_DESCRIPTOR_HANDLE(srvHeap->GetCPUDescriptorHandleForHeapStart(), srvCount, dhIncrSize));
		srvCount++;
		twistsBuffer.createSrv(device, CD3DX12_CPU_DESCRIPTOR_HANDLE(srvHeap->GetCPUDescriptorHandleForHeapStart(), srvCount, dhIncrSize));
		srvCount++;
	}

	virtual void LoadAssets() override {
		using namespace Egg::Scene;
		using namespace Egg::Mesh;

		__super::LoadAssets();

		lua_pushlightuserdata(luaState, (void*)this);
		lua_pushcclosure(luaState, [](lua_State* L) -> int {
			((TreeApp*)lua_touserdata(L, lua_upvalueindex(1)))->LuaAddTreeBuffers();
			return 0;
		}, 1);
		lua_setglobal(luaState, "addTreeBuffers");

		treeMaterialCb.CreateResources(device.Get());

		treeMaterialCb.data.rigging[0] = float4x4::Identity;
		treeMaterialCb.data.rigging[1] = (
			float4x4::Scaling(float3(0.707, 0.707, 0.707))
			*
			float4x4::Rotation(float3(0, 1, 0), 0.785)
			*
			float4x4::Translation(float3(1.5, 0, 3))
			).Transpose().Invert();

		treeMaterialCb.data.rigging[2] = (
			float4x4::Scaling(float3(0.707, 0.707, 0.707))
			*
			float4x4::Rotation(float3(0, 1, 0), -0.785)
			*
			float4x4::Translation(float3(-1.5, 0, 3))
			).Transpose().Invert();
		//treeMaterialCb.data.lineSize    = { 0.2f, 0.06f };
		//treeMaterialCb.data.fading      = { 1.0f, 1.0f };
		//treeMaterialCb.data.texScale    = { 0.3f, 0.3f, 0.3f, 0.3f };
		//treeMaterialCb.data.crossAngle  = { 0.0f, 0.125f, 0.25f, 0.375f };
		//treeMaterialCb.data.stripWidth  = 0.005f;
		//treeMaterialCb.data.overdraw    = 1.0f;
		treeMaterialCb.data.time = 0;

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

		SceneUploadResources();

		BakeBillboardTexture();
		CreateBillboardInstances();
	}

	void recordComputeCommands() {
		auto& cmd = computeCommandLists[swapChainBackBufferIndex];
		ID3D12DescriptorHeap* pHeaps[] = { uavHeap.Get() };
		cmd->SetDescriptorHeaps(_countof(pHeaps), pHeaps);
		D3D12_GPU_DESCRIPTOR_HANDLE heap0 = uavHeap->GetGPUDescriptorHandleForHeapStart();


		growCS.setup(cmd, heap0, 0);
		cmd->SetComputeRootConstantBufferView(0, treeMaterialCb.GetGPUVirtualAddress());
		cmd->Dispatch(nTreeModels, 1, 1);
		D3D12_RESOURCE_BARRIER uavBarriers[] = {
			instanceBuffers[0].uavBarrier(),
			bonesBuffer.uavBarrier(),
			twistsBuffer.uavBarrier()
		};
		cmd->ResourceBarrier(_countof(uavBarriers), uavBarriers);

	}

	virtual void Render() override {
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
		PopulateCommandList();

		ID3D12CommandList* cLists[] = { commandList.Get() };
		commandQueue->ExecuteCommandLists(_countof(cLists), cLists);

		graphicsFenceChain.signal(commandQueue, swapChainBackBufferIndex);

		DX_API("Failed to present swap chain")
			swapChain->Present(1, 0);

		WaitForPreviousFrame();

		frameCount++;
	}

	virtual void PopulateCommandList() override {
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
				CD3DX12_RESOURCE_BARRIER::Transition(twistsBuffer.getResource(),
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
					D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
				CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[swapChainBackBufferIndex].Get(),
					D3D12_RESOURCE_STATE_PRESENT,
					D3D12_RESOURCE_STATE_RENDER_TARGET),
			};
			commandList->ResourceBarrier(4, barriers);
		}

		CD3DX12_CPU_DESCRIPTOR_HANDLE rHandle(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), swapChainBackBufferIndex, rtvDescriptorHandleIncrementSize);
		CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(dsvHeap->GetCPUDescriptorHandleForHeapStart());

		commandList->RSSetViewports(1, &viewPort);
		commandList->RSSetScissorRects(1, &scissorRect);
		commandList->OMSetRenderTargets(1, &rHandle, FALSE, &dsvHandle);

		const float clearColor[] = { 0.5f, 0.6f, 0.9f, 1.0f };
		commandList->ClearRenderTargetView(rHandle, clearColor, 0, nullptr);
		commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		__super::PopulateCommandList();

		RecordBillboardDraw();

		{
			D3D12_RESOURCE_BARRIER barriers[] = {
				CD3DX12_RESOURCE_BARRIER::Transition(instanceBuffers[0].getResource(),
					D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
				CD3DX12_RESOURCE_BARRIER::Transition(bonesBuffer.getResource(),
					D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
				CD3DX12_RESOURCE_BARRIER::Transition(twistsBuffer.getResource(),
					D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
			};
			commandList->ResourceBarrier(3, barriers);
		}

		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[swapChainBackBufferIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

		DX_API("close graphics command list")
			commandList->Close();
	}

	virtual void Resize(int width, int height) override {
		Egg::Script::ScriptedApp::Resize(width, height);
	}

	virtual void Update(float dt, float T) override {
		__super::Update(dt, T);

		treeMaterialCb.data.time += dt;
		treeMaterialCb.Upload();
	}

	virtual void ProcessMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override {
		if (!guiHwnd) createGui();
		else if (uMsg == WM_KEYUP) {
			if (wParam == 49) {
			}
			if (wParam == 50) {
			}
			if (wParam == 51) {
			}
			if (wParam == 52) {
			}
		}

		__super::ProcessMessage(hWnd, uMsg, wParam, lParam);
	}

	virtual void ReleaseSwapChainResources() override {
		__super::ReleaseSwapChainResources();
	}

	virtual void ReleaseResources() override {
		if (guiHwnd) { DestroyWindow(guiHwnd); guiHwnd = nullptr; }
		treeMaterialCb.ReleaseResources();
		dispatchArgsResource.Reset();
		dispatchCommandSignature.Reset();
		drawCommandSignature.Reset();
		uavHeap.Reset();
		billboardAtlas.Reset();
		billboardAtlasSrvHeap.Reset();
		billboardPositionsBuffer.Reset();
		billboardRenderSrvHeap.Reset();
		billboardRenderRootSig.Reset();
		billboardRenderPso.Reset();
		Egg::Script::ScriptedApp::ReleaseResources();
	}
};
