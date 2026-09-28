#pragma once
#include "Egg/Common.h"
#include <Egg/OpenXR/OpenXRApp.h>
#include <Egg/Shader.h>
#include <Egg/ConstantBuffer.hpp>
#include <Egg/Scene/ConstantBufferTypes.h>
#include <Egg/Mesh/Material.h>
#include <Egg/Mesh/Shaded.h>
#include <Egg/Mesh/Geometry.h>
#include <Egg/Importer.h>
#include <Egg/Compute/RawBuffer.h>
#include <Egg/Compute/TypedBuffer.h>
#include <Egg/Compute/ComputeShader.h>

#include "../g-Retam/Shaders/Retam/RetamCb.hlsli"

// VR port of g-Retam's stroke pipeline onto Egg::OpenXRApp. Ports the CORE
// pipeline (collect -> GPU sort/compact/extract-strokes -> cubic stroke
// extrude) faithfully from RetamApp.h, but bypasses ScriptedApp/Lua entirely
// -- OpenXRApp is not a ManagerApp/ScriptedApp (different base class chain),
// so the scene here is a single hardcoded knight mesh built by calling the
// same underlying Egg::Mesh::Material/Shaded/Importer C++ APIs the Lua
// bindings themselves wrap, instead of going through Lua text.
//
// Deliberately out of scope for this port (see chat): the native Win32 GUI
// (retamMaterialCb keeps RetamApp's default tuning values, no live sliders),
// the "ReCollect" debug material/mien=3 pass (off by default even on
// desktop), and multiple entities -- just the one knight.
//
// Unlike RetamApp's desktop-stereo path, there is no separate compute queue
// and no hard CPU/GPU sync between collect/compute/draw: OpenXRApp hands
// PopulateEyeCommandList() ONE already-open direct command list per eye, and
// a direct command list can record compute Dispatches too, so the whole
// per-eye pipeline is one continuous recording separated by ordinary UAV/
// resource barriers -- correct because everything stays in one command
// list on one queue, executing in submitted order. The two-eyes-sharing-
// single-buffered-resources concern RetamApp's HardSync() existed for is
// instead handled for free by OpenXRApp::Render()'s own per-eye Execute+
// WaitForGpu before the next eye's command list is even Reset().
class RetamVrApp : public Egg::OpenXRApp {
protected:
	Egg::ConstantBuffer<RetamMaterialCb> retamMaterialCb;
	Egg::ConstantBuffer<PerFrameCb> perFrameCb;
	Egg::ConstantBuffer<PerObjectCb> perObjectCb;

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
	com_ptr<ID3D12CommandSignature>  drawCommandSignature;

	Egg::Compute::ComputeShader sortCS;
	Egg::Compute::ComputeShader prefixSumCS;
	Egg::Compute::ComputeShader compactCS;
	Egg::Compute::ComputeShader argsCS;
	Egg::Compute::ComputeShader cubicCS;

	com_ptr<ID3D12RootSignature> cubicExtrudeRootSig;
	com_ptr<ID3D12PipelineState> cubicExtrudePSO;

	// 1024x1024 UV-space fragment-collect target -- same role as
	// RetamApp's collectDepthBuffer/collectColorBuffer, reused across both
	// eyes each frame (cleared each use, single-buffered like everything
	// else here).
	com_ptr<ID3D12Resource> collectDepthBuffer;
	com_ptr<ID3D12DescriptorHeap> collectDsvHeap;
	com_ptr<ID3D12Resource> collectColorBuffer;
	com_ptr<ID3D12DescriptorHeap> collectRtvHeap;

	Egg::Mesh::Geometry::P knightGeometry;
	Egg::Mesh::Material::P retam256Material, retam256CollectMaterial, layDownDepthMaterial;
	Egg::Mesh::Shaded::P   retam256Shaded, retam256CollectShaded, layDownDepthShaded;

	float rotationAngle = 0.0f;
	float timeT = 0.0f;

	com_ptr<ID3D12Resource> CreateUploadBuffer(const void* data, size_t sizeBytes) {
		com_ptr<ID3D12Resource> res;
		DX_API("Failed to create upload buffer")
			device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(sizeBytes),
				D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(res.GetAddressOf()));
		void* mapped = nullptr;
		CD3DX12_RANGE readRange(0, 0);
		res->Map(0, &readRange, &mapped);
		memcpy(mapped, data, sizeBytes);
		res->Unmap(0, nullptr);
		return res;
	}

public:
	RetamVrApp() : Egg::OpenXRApp(),
		fragmentCountsBuffer(L"fragmentCounts", 16 * 1024),
		fragmentsBuffer(L"fragments", 1024u * 1024u * 16u),
		designBuffer(L"design", 16u * 5u * 1024u * 16u, DXGI_FORMAT_R32G32B32A32_FLOAT),
		cubicBuffer(L"cubic", 1024u * 16u * 16u * 4u, DXGI_FORMAT_R32G32B32A32_FLOAT),
		strokeCountsBuffer(L"strokeCounts", 1024u * 16u),
		strokeOffsetsBuffer(L"strokeOffsets", 1024u * 16u),
		strokeListBuffer(L"strokeList", 1024u * 16u * 16u),
		debugBuffer(L"debug", 1024u * 1024u * 16u)
	{}

	virtual void Update(float dt, float T) override {
		timeT = T;
		rotationAngle = T * 0.4f;
	}

	virtual void CreateResources() override {
		Egg::OpenXRApp::CreateResources();

		D3D12_DESCRIPTOR_HEAP_DESC dhd = {};
		dhd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		dhd.NumDescriptors = 12; // 0-8 buffers/dispatchArgs, 9 cubicBuffer SRV, 10 uvmask, 11 gayline
		dhd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		DX_API("create descriptor heap for uavs")
			device->CreateDescriptorHeap(&dhd, IID_PPV_ARGS(uavHeap.GetAddressOf()));
		uint dhIncrSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		CD3DX12_CPU_DESCRIPTOR_HANDLE countsHandle(uavHeap->GetCPUDescriptorHandleForHeapStart(), 0, dhIncrSize);
		fragmentCountsBuffer.createResources(device, countsHandle);
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
			psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
			psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
			psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
			psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
			psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
			psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
			psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
			psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
			psoDesc.NumRenderTargets = 1;
			psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // must match CreateXrSwapchains()'s format
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
	}

	virtual void CreateSwapChainResources() override {
		Egg::OpenXRApp::CreateSwapChainResources();

		D3D12_DESCRIPTOR_HEAP_DESC collectDsvHeapDesc = {};
		collectDsvHeapDesc.NumDescriptors = 1;
		collectDsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		DX_API("create collect DSV heap")
			device->CreateDescriptorHeap(&collectDsvHeapDesc, IID_PPV_ARGS(collectDsvHeap.ReleaseAndGetAddressOf()));

		D3D12_CLEAR_VALUE collectDepthClearVal = {};
		collectDepthClearVal.Format = DXGI_FORMAT_D32_FLOAT;
		collectDepthClearVal.DepthStencil.Depth = 1.0f;
		DX_API("create collect depth buffer")
			device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, 1024, 1024, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
				D3D12_RESOURCE_STATE_DEPTH_WRITE, &collectDepthClearVal,
				IID_PPV_ARGS(collectDepthBuffer.ReleaseAndGetAddressOf()));

		D3D12_DEPTH_STENCIL_VIEW_DESC collectDsvDesc = {};
		collectDsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
		collectDsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		device->CreateDepthStencilView(collectDepthBuffer.Get(), &collectDsvDesc, collectDsvHeap->GetCPUDescriptorHandleForHeapStart());

		D3D12_DESCRIPTOR_HEAP_DESC collectRtvHeapDesc = {};
		collectRtvHeapDesc.NumDescriptors = 1;
		collectRtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		DX_API("create collect RTV heap")
			device->CreateDescriptorHeap(&collectRtvHeapDesc, IID_PPV_ARGS(collectRtvHeap.ReleaseAndGetAddressOf()));

		D3D12_CLEAR_VALUE collectColorClearVal = {};
		collectColorClearVal.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		DX_API("create collect color buffer")
			device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, 1024, 1024, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),
				D3D12_RESOURCE_STATE_RENDER_TARGET, &collectColorClearVal,
				IID_PPV_ARGS(collectColorBuffer.ReleaseAndGetAddressOf()));

		D3D12_RENDER_TARGET_VIEW_DESC collectRtvDesc = {};
		collectRtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		collectRtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		device->CreateRenderTargetView(collectColorBuffer.Get(), &collectRtvDesc, collectRtvHeap->GetCPUDescriptorHandleForHeapStart());
	}

	virtual void LoadAssets() override {
		uint dhIncrSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		retamMaterialCb.CreateResources(device.Get());
		retamMaterialCb.data.lineSize   = { 0.2f, 0.06f };
		retamMaterialCb.data.fading     = { 1.0f, 1.0f };
		retamMaterialCb.data.texScale   = { 0.3f, 0.3f, 0.3f, 0.3f };
		retamMaterialCb.data.crossAngle = { 0.0f, 0.125f, 0.25f, 0.375f };
		retamMaterialCb.data.stripWidth = 0.005f;
		retamMaterialCb.data.overdraw   = 1.0f;
		retamMaterialCb.Upload();

		perFrameCb.CreateResources(device.Get());
		perObjectCb.CreateResources(device.Get());

		// VR-appropriate scale: real-world meters, not the desktop scene's
		// enormous (~207m) torus scale -- see chat for the measured-height
		// story. 0.6m tall knight, comfortably in front of the headset's
		// starting pose.
		knightGeometry = Egg::Importer::ImportWithTangentSpace(device.Get(), "chess/knight.obj", 0.6f);

		// Textures into uavHeap: slot 10 = uvmask (only read by
		// retam256CollectPS via a root-parameter slot RetamApp's own
		// desktop LoadAssets() overwrites for UAV access instead -- a
		// harmless pre-existing quirk masked by dead code there, ported
		// faithfully rather than "fixed" blind). Slot 11 = gayline (the
		// actual ink stroke texture retam256/layDownDepth need correctly
		// bound).
		LoadTexture2D("textures/uvmask.png", 10);
		LoadTexture2D("gayline.png", 11);

		SceneUploadResources();

		// -- retam256 (mien 0 equivalent): the final visible base pass --
		{
			com_ptr<ID3DBlob> vs = Egg::Shader::LoadCso("Shaders/Retam/retamVS.cso");
			com_ptr<ID3DBlob> ps = Egg::Shader::LoadCso("Shaders/Retam/retam256PS.cso");
			com_ptr<ID3D12RootSignature> rootSig = Egg::Shader::LoadRootSignature(device.Get(), vs.Get());

			retam256Material = Egg::Mesh::Material::Create();
			retam256Material->SetRootSignature(rootSig);
			retam256Material->SetVertexShader(vs);
			retam256Material->SetPixelShader(ps);
			retam256Material->SetDepthStencilState(CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT));
			retam256Material->SetDSVFormat(DXGI_FORMAT_D32_FLOAT);
			retam256Material->SetConstantBuffer(perObjectCb, sizeof(Egg::Scene::PerObjectData));
			retam256Material->SetConstantBuffer(perFrameCb);
			retam256Material->SetConstantBuffer(retamMaterialCb);
			retam256Material->SetSrvHeap(3, uavHeap, 11 * dhIncrSize);

			retam256Shaded = Egg::Mesh::Shaded::Create(psoManager, retam256Material, knightGeometry,
				std::vector<DXGI_FORMAT>{ DXGI_FORMAT_R8G8B8A8_UNORM_SRGB });
		}

		// -- layDownDepth (mien 2 equivalent): depth-only prepass, no RTV --
		{
			com_ptr<ID3DBlob> vs = Egg::Shader::LoadCso("Shaders/Retam/retamVS.cso");
			com_ptr<ID3DBlob> ps = Egg::Shader::LoadCso("Shaders/Retam/layDownDepthPS.cso");
			com_ptr<ID3D12RootSignature> rootSig = Egg::Shader::LoadRootSignature(device.Get(), vs.Get());

			layDownDepthMaterial = Egg::Mesh::Material::Create();
			layDownDepthMaterial->SetRootSignature(rootSig);
			layDownDepthMaterial->SetVertexShader(vs);
			layDownDepthMaterial->SetPixelShader(ps);
			layDownDepthMaterial->SetDepthStencilState(CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT));
			layDownDepthMaterial->SetDSVFormat(DXGI_FORMAT_D32_FLOAT);
			layDownDepthMaterial->SetConstantBuffer(perObjectCb, sizeof(Egg::Scene::PerObjectData));
			layDownDepthMaterial->SetConstantBuffer(perFrameCb);
			layDownDepthMaterial->SetSrvHeap(3, uavHeap, 11 * dhIncrSize);

			layDownDepthShaded = Egg::Mesh::Shaded::Create(psoManager, layDownDepthMaterial, knightGeometry,
				std::vector<DXGI_FORMAT>{});
		}

		// -- retam256Collect (mien 1 equivalent): fragment collection into UAV buffers --
		{
			com_ptr<ID3DBlob> vs = Egg::Shader::LoadCso("Shaders/Retam/retamCollectVS.cso");
			com_ptr<ID3DBlob> ps = Egg::Shader::LoadCso("Shaders/Retam/retam256CollectPS.cso");
			com_ptr<ID3D12RootSignature> rootSig = Egg::Shader::LoadRootSignature(device.Get(), vs.Get());

			retam256CollectMaterial = Egg::Mesh::Material::Create();
			retam256CollectMaterial->SetRootSignature(rootSig);
			retam256CollectMaterial->SetVertexShader(vs);
			retam256CollectMaterial->SetPixelShader(ps);
			retam256CollectMaterial->SetDepthStencilState(CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT));
			retam256CollectMaterial->SetDepthCompLessEqual();
			retam256CollectMaterial->SetDSVFormat(DXGI_FORMAT_D32_FLOAT);
			retam256CollectMaterial->SetConstantBuffer(perObjectCb, sizeof(Egg::Scene::PerObjectData));
			retam256CollectMaterial->SetConstantBuffer(perFrameCb);
			retam256CollectMaterial->SetConstantBuffer(retamMaterialCb);
			retam256CollectMaterial->SetSrvHeap(3, uavHeap, 0); // UAV(u0,u1) table -- fragmentCounts/fragments

			retam256CollectShaded = Egg::Mesh::Shaded::Create(psoManager, retam256CollectMaterial, knightGeometry,
				std::vector<DXGI_FORMAT>{ DXGI_FORMAT_R8G8B8A8_UNORM });
		}

		// Single static object: knight at (0,0,-1.2), spun slowly for visual
		// interest (see Update()). modelTransform rebuilt per-frame below.
	}

	virtual void ReleaseAssets() override {
		retam256Shaded = nullptr; layDownDepthShaded = nullptr; retam256CollectShaded = nullptr;
		retam256Material = nullptr; layDownDepthMaterial = nullptr; retam256CollectMaterial = nullptr;
		knightGeometry = nullptr;
	}

private:
	Egg::Texture2D LoadTexture2D(const std::string& filename, unsigned int uavHeapSlot) {
		std::string path = "../Media/" + filename;
		Egg::Texture2D tex = Egg::Importer::ImportTexture2D(device.Get(), path);
		uint dhIncrSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		CD3DX12_CPU_DESCRIPTOR_HANDLE handle(uavHeap->GetCPUDescriptorHandleForHeapStart(), uavHeapSlot, dhIncrSize);
		tex.CreateSRV(device.Get(), uavHeap.Get(), uavHeapSlot);
		pendingTextureUploads.push_back(tex);
		return tex;
	}

	std::vector<Egg::Texture2D> pendingTextureUploads;

	void SceneUploadResources() {
		DX_API("Failed to reset command allocator (UploadResources)")
			commandAllocator->Reset();
		DX_API("Failed to reset command list (UploadResources)")
			commandList->Reset(commandAllocator.Get(), nullptr);

		for (auto& tex : pendingTextureUploads)
			tex.UploadResource(commandList.Get());

		DX_API("Failed to close command list (UploadResources)")
			commandList->Close();
		ID3D12CommandList* lists[] = { commandList.Get() };
		commandQueue->ExecuteCommandLists(1, lists);

		DX_API("Failed to signal fence") commandQueue->Signal(fence.Get(), ++fenceValue);
		DX_API("Failed to register fence completion") fence->SetEventOnCompletion(fenceValue, fenceEvent);
		WaitForSingleObject(fenceEvent, INFINITE);

		for (auto& tex : pendingTextureUploads)
			tex.ReleaseUploadResources();
		pendingTextureUploads.clear();
	}

protected:
	// Records the entire per-eye retam pipeline (collect -> compute ->
	// cubic stroke extrude) into the single already-open command list
	// OpenXRApp::Render() gives us for this eye.
	virtual void PopulateEyeCommandList() override {
		using namespace Egg::Math;

		// -- Per-eye camera: real headset pose/FOV, not StereoCamera.h's
		// eyeSeparation-slider math -- xrViews[eye]/eyeViewMatrix/
		// eyeProjMatrix are already built by OpenXRApp::BuildEyeMatrices()
		// before PopulateEyeCommandList() is called for either eye.
		float3 eyePos(xrViews[xrCurrentEye].pose.position.x,
			xrViews[xrCurrentEye].pose.position.y,
			xrViews[xrCurrentEye].pose.position.z);

		// World-space forward: rotate the view-space forward axis (+Z --
		// see XrFovToProjectionMatrix's w_clip=-v_z comment, and the
		// verified-by-construction view matrix, which puts what's in front
		// of the eye at positive view-space Z) by the eye's own pose
		// quaternion. retam256PS.hlsl's stroke-density/LOD calculation
		// (dot(viewDiff, -ahead.xyz)) genuinely depends on this being the
		// real per-eye heading, not a fixed world axis.
		const auto& q = xrViews[xrCurrentEye].pose.orientation;
		float3 qv(q.x, q.y, q.z);
		float3 localForward(0.0f, 0.0f, 1.0f);
		float3 t = qv.Cross(localForward) * 2.0f;
		float3 worldAhead = localForward + t * q.w + qv.Cross(t);

		perFrameCb->viewProjTransform = eyeViewMatrix[xrCurrentEye] * eyeProjMatrix[xrCurrentEye];
		perFrameCb->cameraPos = float4(eyePos, 1.0f);
		perFrameCb->ahead = float4(worldAhead, 0.0f);
		perFrameCb->time = float4(timeT, 0, 0, 0);
		perFrameCb.Upload();

		float4x4 world = float4x4::Rotation(float3::UnitY, rotationAngle) * float4x4::Translation(float3(0.0f, 0.0f, -1.2f));
		perObjectCb->objects[0].modelTransform = world;
		perObjectCb->objects[0].modelTransformInverse = world.Invert();
		perObjectCb.Upload();

		CD3DX12_CPU_DESCRIPTOR_HANDLE collectDsvHandle(collectDsvHeap->GetCPUDescriptorHandleForHeapStart());
		CD3DX12_CPU_DESCRIPTOR_HANDLE collectRtvHandle(collectRtvHeap->GetCPUDescriptorHandleForHeapStart());

		// --- Pass 1: lay down depth + collect fragments (1024x1024 UV space) ---
		D3D12_VIEWPORT collectViewPort = { 0.0f, 0.0f, 1024.0f, 1024.0f, 0.0f, 1.0f };
		D3D12_RECT collectScissor = { 0, 0, 1024, 1024 };
		commandList->RSSetViewports(1, &collectViewPort);
		commandList->RSSetScissorRects(1, &collectScissor);

		fragmentCountsBuffer.upload(commandList);
		commandList->OMSetRenderTargets(0, nullptr, FALSE, &collectDsvHandle);
		commandList->ClearDepthStencilView(collectDsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		layDownDepthShaded->Draw(commandList.Get(), 0);

		const float collectClearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		commandList->ClearRenderTargetView(collectRtvHandle, collectClearColor, 0, nullptr);
		commandList->OMSetRenderTargets(1, &collectRtvHandle, FALSE, &collectDsvHandle);
		retam256CollectShaded->Draw(commandList.Get(), 0);

		{
			D3D12_RESOURCE_BARRIER b[] = { fragmentCountsBuffer.uavBarrier(), fragmentsBuffer.uavBarrier() };
			commandList->ResourceBarrier(2, b);
		}

		// --- Pass 2: GPU sort/compact/extract strokes from the fragments just collected ---
		ID3D12DescriptorHeap* pHeaps[] = { uavHeap.Get() };
		commandList->SetDescriptorHeaps(_countof(pHeaps), pHeaps);
		D3D12_GPU_DESCRIPTOR_HANDLE heap0 = uavHeap->GetGPUDescriptorHandleForHeapStart();

		strokeCountsBuffer.upload(commandList);
		strokeOffsetsBuffer.upload(commandList);

		sortCS.setup(commandList, heap0, 0);
		commandList->Dispatch(1024 * 16, 1, 1);
		commandList->ResourceBarrier(1, &strokeCountsBuffer.uavBarrier());

		prefixSumCS.setup(commandList, heap0, 0);
		commandList->Dispatch(1, 1, 1);
		{
			D3D12_RESOURCE_BARRIER b[] = { strokeOffsetsBuffer.uavBarrier(), designBuffer.uavBarrier() };
			commandList->ResourceBarrier(2, b);
		}

		compactCS.setup(commandList, heap0, 0);
		commandList->Dispatch(1024 * 16, 1, 1);

		argsCS.setup(commandList, heap0, 0);
		commandList->Dispatch(1, 1, 1);

		{
			D3D12_RESOURCE_BARRIER argsUAV = {};
			argsUAV.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			argsUAV.UAV.pResource = dispatchArgsResource.Get();
			D3D12_RESOURCE_BARRIER b[] = { strokeListBuffer.uavBarrier(), argsUAV };
			commandList->ResourceBarrier(2, b);
		}
		{
			D3D12_RESOURCE_BARRIER toIndirect = CD3DX12_RESOURCE_BARRIER::Transition(
				dispatchArgsResource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
			commandList->ResourceBarrier(1, &toIndirect);
		}

		cubicCS.setup(commandList, heap0, 0);
		commandList->ExecuteIndirect(dispatchCommandSignature.Get(), 1, dispatchArgsResource.Get(), 0, nullptr, 0);

		{
			D3D12_RESOURCE_BARRIER fromIndirect = CD3DX12_RESOURCE_BARRIER::Transition(
				dispatchArgsResource.Get(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			commandList->ResourceBarrier(1, &fromIndirect);
		}
		{
			D3D12_RESOURCE_BARRIER b[] = { debugBuffer.uavBarrier(), cubicBuffer.uavBarrier() };
			commandList->ResourceBarrier(2, b);
		}

		// --- Pass 3: final draw -- base pass + cubic-extruded strokes, into this eye's XR swapchain image ---
		D3D12_CPU_DESCRIPTOR_HANDLE eyeRtv = GetEyeRtv(xrCurrentEye, xrCurrentImageIndex[xrCurrentEye]);
		D3D12_CPU_DESCRIPTOR_HANDLE eyeDsv = GetEyeDsv(xrCurrentEye);

		commandList->RSSetViewports(1, &eyeViewports[xrCurrentEye]);
		commandList->RSSetScissorRects(1, &eyeScissorRects[xrCurrentEye]);
		commandList->OMSetRenderTargets(1, &eyeRtv, FALSE, &eyeDsv);

		const float whiteClear[] = { 1.0f, 1.0f, 1.0f, 1.0f }; // white background, matching g-Retam's stereo work
		commandList->ClearRenderTargetView(eyeRtv, whiteClear, 0, nullptr);
		commandList->ClearDepthStencilView(eyeDsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		retam256Shaded->Draw(commandList.Get(), 0);

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
			CD3DX12_GPU_DESCRIPTOR_HANDLE carrotSrvGpu(uavHeap->GetGPUDescriptorHandleForHeapStart(), 11, dhIncrSize);
			commandList->SetGraphicsRootSignature(cubicExtrudeRootSig.Get());
			commandList->SetGraphicsRootDescriptorTable(0, cubicSrvGpu);
			commandList->SetGraphicsRootDescriptorTable(1, carrotSrvGpu);
			commandList->SetGraphicsRootConstantBufferView(2, retamMaterialCb.GetGPUVirtualAddress());
			commandList->SetPipelineState(cubicExtrudePSO.Get());
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
			commandList->OMSetRenderTargets(1, &eyeRtv, FALSE, &eyeDsv);
			commandList->ExecuteIndirect(drawCommandSignature.Get(), 1, dispatchArgsResource.Get(), 12, nullptr, 0);
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
	}

	virtual void ReleaseSwapChainResources() override {
		collectDepthBuffer.Reset();
		collectDsvHeap.Reset();
		collectColorBuffer.Reset();
		collectRtvHeap.Reset();
		Egg::OpenXRApp::ReleaseSwapChainResources();
	}

	virtual void ReleaseResources() override {
		retamMaterialCb.ReleaseResources();
		perFrameCb.ReleaseResources();
		perObjectCb.ReleaseResources();
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
		Egg::OpenXRApp::ReleaseResources();
	}
};
