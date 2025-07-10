#pragma once


#include "ConstantBufferTypes.h"
#include <vector>
#include <algorithm>

#include <Egg/Control/ControlApp.h>
#include "Particle.h"
#include <vector>
#include <algorithm>



using namespace Egg::Math;

class ggl013App : public Egg::Control::ControlApp {
protected:

	com_ptr<ID3D12Resource> shadowMapBuffer;
	com_ptr<ID3D12DescriptorHeap> shadowMapRtvHeap;
	com_ptr<ID3D12DescriptorHeap> shadowMapSrvHeap;

	com_ptr<ID3D12Resource> shadowMapDepthBuffer;
	com_ptr<ID3D12DescriptorHeap> shadowMapDsvHeap;

	com_ptr<ID3DBlob> shadowMapVertexShader;
	com_ptr<ID3DBlob> shadowMapGeometryShader;
	com_ptr<ID3DBlob> shadowMapPixelShader;
	com_ptr<ID3D12RootSignature> shadowMapRootSig;

	com_ptr<ID3DBlob> shadowedVertexShader;
	com_ptr<ID3DBlob> shadowedPixelShader;
	com_ptr<ID3D12RootSignature> shadowedRootSig;

	std::vector<Particle> particles;
	Egg::Mesh::VertexStreamGeometry::P particlesGeometry;
	Egg::Mesh::Shaded::P fireBillboardSet;
	com_ptr<ID3D12DescriptorHeap> particleSrvHeap;

public:
	virtual void CreateResources() override {
		__super::CreateResources();

		D3D12_DESCRIPTOR_HEAP_DESC dhd;

		dhd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		dhd.NodeMask = 0;
		dhd.NumDescriptors = 1024;
		dhd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

		DX_API("Failed to create descriptor heap for texture")
			device->CreateDescriptorHeap(&dhd, IID_PPV_ARGS(particleSrvHeap.GetAddressOf()));

		D3D12_DESCRIPTOR_HEAP_DESC shadowMapHeapDesc = {};
		shadowMapHeapDesc.NumDescriptors = 1;
		shadowMapHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		shadowMapHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		DX_API("Failed to create shadowMap descriptor heap")
			device->CreateDescriptorHeap(&shadowMapHeapDesc, IID_PPV_ARGS(shadowMapRtvHeap.GetAddressOf()));

		D3D12_CLEAR_VALUE shadowMapClearValue = {};
		shadowMapClearValue.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		shadowMapClearValue.Color[0] = 10000.0f;
		shadowMapClearValue.Color[1] = 0.0f;
		shadowMapClearValue.Color[2] = 0.0f;
		shadowMapClearValue.Color[3] = 0.0f;

		DX_API("Failed to create shadowMap buffer")
			device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT,
					1024,
					1024, 2/*nLights*/, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				&shadowMapClearValue,
				IID_PPV_ARGS(shadowMapBuffer.GetAddressOf()));

		shadowMapBuffer->SetName(L"shadowMap Buffer");

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
		rtvDesc.Texture2DArray.MipSlice = 0;
		rtvDesc.Texture2DArray.PlaneSlice = 0;
		rtvDesc.Texture2DArray.FirstArraySlice = 0;
		rtvDesc.Texture2DArray.ArraySize = 2/*nLights*/;

		device->CreateRenderTargetView(shadowMapBuffer.Get(), &rtvDesc, shadowMapRtvHeap->GetCPUDescriptorHandleForHeapStart());

		//D3D12_DESCRIPTOR_HEAP_DESC shadowMapSrvHeapDesc = {};
		//shadowMapSrvHeapDesc.NumDescriptors = 1;
		//shadowMapSrvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		//shadowMapSrvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		//DX_API("Failed to create shadowMap srv descriptor heap")
		//	device->CreateDescriptorHeap(&shadowMapSrvHeapDesc, IID_PPV_ARGS(shadowMapSrvHeap.GetAddressOf()));

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		srvDesc.Texture2DArray.MipLevels = 1;
		srvDesc.Texture2DArray.MostDetailedMip = 0;
		srvDesc.Texture2DArray.FirstArraySlice = 0;
		srvDesc.Texture2DArray.ArraySize = 2;
		srvDesc.Texture2DArray.PlaneSlice = 0;
		srvDesc.Texture2DArray.ResourceMinLODClamp = 0;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

//		auto cpuHandle = shadowMapSrvHeap->GetCPUDescriptorHandleForHeapStart();
		auto cpuHandle = srvHeap->GetCPUDescriptorHandleForHeapStart();
		static unsigned int incrementSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		CD3DX12_CPU_DESCRIPTOR_HANDLE handle{ cpuHandle, (int)(2 * incrementSize) };
		device->CreateShaderResourceView(shadowMapBuffer.Get(), &srvDesc, handle);

		// ---- depth

		D3D12_DESCRIPTOR_HEAP_DESC dsHeapDesc = {};
		dsHeapDesc.NumDescriptors = 1;
		dsHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		dsHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		DX_API("Failed to create depth stencil descriptor heap")
			device->CreateDescriptorHeap(&dsHeapDesc, IID_PPV_ARGS(shadowMapDsvHeap.GetAddressOf()));

		D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
		depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
		depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
		depthOptimizedClearValue.DepthStencil.Stencil = 0;

		DX_API("Failed to create Depth Stencil buffer")
			device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT,
					1024, 1024, 2/*nLights*/, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
				D3D12_RESOURCE_STATE_DEPTH_WRITE,
				&depthOptimizedClearValue,
				IID_PPV_ARGS(shadowMapDepthBuffer.GetAddressOf()));

		shadowMapDepthBuffer->SetName(L"Shadow Map Depth Stencil Buffer");

		D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
		depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
		depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
		depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;
		depthStencilDesc.Texture2DArray.ArraySize = 2/*nLights*/;
		depthStencilDesc.Texture2DArray.FirstArraySlice = 0;
		depthStencilDesc.Texture2DArray.MipSlice = 0;

		device->CreateDepthStencilView(shadowMapDepthBuffer.Get(), &depthStencilDesc, shadowMapDsvHeap->GetCPUDescriptorHandleForHeapStart());

	}

	virtual void PopulateCommandList() override {
		commandAllocator->Reset();
		commandList->Reset(commandAllocator.Get(), nullptr);

		D3D12_VIEWPORT shadowVp;
		shadowVp.TopLeftX = 0;
		shadowVp.TopLeftY = 0;
		shadowVp.Height = 1024;
		shadowVp.Width = 1024;
		shadowVp.MinDepth = 0.0f;
		shadowVp.MaxDepth = 1.0f;
		D3D12_RECT shadowSci;
		shadowSci.top = 0;
		shadowSci.left = 0;
		shadowSci.right = 1024;
		shadowSci.bottom = 1024;

		commandList->RSSetViewports(1, &shadowVp);
		commandList->RSSetScissorRects(1, &shadowSci);

		commandList->ResourceBarrier(1, 
			&CD3DX12_RESOURCE_BARRIER::Transition(shadowMapBuffer.Get(),
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_RENDER_TARGET));

		CD3DX12_CPU_DESCRIPTOR_HANDLE shadowMapHandle(
			shadowMapRtvHeap->GetCPUDescriptorHandleForHeapStart(), 0,
			rtvDescriptorHandleIncrementSize);
		CD3DX12_CPU_DESCRIPTOR_HANDLE shadowMapDsvHandle(shadowMapDsvHeap->GetCPUDescriptorHandleForHeapStart());
		commandList->OMSetRenderTargets(1, &shadowMapHandle, FALSE, &shadowMapDsvHandle);

		const float shadowMapClearColor[] = { 10000.0f, 0.0f, 0.0f, 0.0f };
		commandList->ClearRenderTargetView(shadowMapHandle, shadowMapClearColor, 0, nullptr);
		commandList->ClearDepthStencilView(shadowMapDsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		for (int i = 0; i < entities.size(); i++) {
			entities[i]->Draw(commandList.Get(), 1, i);
		}

		commandList->ResourceBarrier(1, 
			&CD3DX12_RESOURCE_BARRIER::Transition(shadowMapBuffer.Get(), 
				D3D12_RESOURCE_STATE_RENDER_TARGET, 
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

		//----- SHADOW MAP DONE

		commandList->RSSetViewports(1, &viewPort);
		commandList->RSSetScissorRects(1, &scissorRect);

		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[swapChainBackBufferIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

		CD3DX12_CPU_DESCRIPTOR_HANDLE rHandle(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), swapChainBackBufferIndex, rtvDescriptorHandleIncrementSize);
		CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(dsvHeap->GetCPUDescriptorHandleForHeapStart());
		commandList->OMSetRenderTargets(1, &rHandle, FALSE, &dsvHandle);

		const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
		commandList->ClearRenderTargetView(rHandle, clearColor, 0, nullptr);
		commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		__super::PopulateCommandList();

		fireBillboardSet->Draw(commandList.Get());

		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[swapChainBackBufferIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

		DX_API("Failed to close command list")
			commandList->Close();
	}

	void UploadResources() {
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

		for (int i = 0; i < 400; i++)
			particles.push_back(Particle());

		particlesGeometry =
			Egg::Mesh::VertexStreamGeometry::Create(device.Get(),
				particles.data(),
				sizeof(particles[0]) * particles.size(),
				sizeof(particles[0]));
		particlesGeometry->SetTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);

		D3D12_INPUT_ELEMENT_DESC particlePositionDesc;
		particlePositionDesc.AlignedByteOffset = offsetof(Particle, position);
		particlePositionDesc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
		particlePositionDesc.InputSlot = 0;
		particlePositionDesc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		particlePositionDesc.InstanceDataStepRate = 0;
		particlePositionDesc.SemanticIndex = 0;
		particlePositionDesc.SemanticName = "POSITION";
		particlesGeometry->AddInputElement(particlePositionDesc);

		D3D12_INPUT_ELEMENT_DESC particleLifespanDesc;
		particleLifespanDesc.AlignedByteOffset = offsetof(Particle, lifespan);
		particleLifespanDesc.Format = DXGI_FORMAT_R32_FLOAT;
		particleLifespanDesc.InputSlot = 0;
		particleLifespanDesc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		particleLifespanDesc.InstanceDataStepRate = 0;
		particleLifespanDesc.SemanticIndex = 0;
		particleLifespanDesc.SemanticName = "LIFESPAN";
		particlesGeometry->AddInputElement(particleLifespanDesc);

		D3D12_INPUT_ELEMENT_DESC particleAgeDesc;
		particleAgeDesc.AlignedByteOffset = offsetof(Particle, age);
		particleAgeDesc.Format = DXGI_FORMAT_R32_FLOAT;
		particleAgeDesc.InputSlot = 0;
		particleAgeDesc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		particleAgeDesc.InstanceDataStepRate = 0;
		particleAgeDesc.SemanticIndex = 0;
		particleAgeDesc.SemanticName = "AGE";
		particlesGeometry->AddInputElement(particleAgeDesc);

		com_ptr<ID3DBlob> billboardVertexShader = Egg::Shader::LoadCso("Shaders/billboardVS.cso");
		com_ptr<ID3DBlob> billboardGeometryShader = Egg::Shader::LoadCso("Shaders/billboardGS.cso");
		com_ptr<ID3DBlob> firePixelShader = Egg::Shader::LoadCso("Shaders/firePS.cso");
		com_ptr<ID3D12RootSignature> billboardRootSig = Egg::Shader::LoadRootSignature(device.Get(), billboardVertexShader.Get());

		Egg::Texture2D tex = LoadTexture2D("particle.png");
		tex.CreateSRV(device.Get(), particleSrvHeap.Get(), 0);
		
		D3D12_BLEND_DESC transparency;
		transparency.AlphaToCoverageEnable = false;
		transparency.IndependentBlendEnable = false;
		transparency.RenderTarget[0].BlendEnable = true;
		transparency.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		transparency.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		transparency.RenderTarget[0].LogicOpEnable = false;
		transparency.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
		transparency.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		transparency.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		transparency.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		transparency.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
		transparency.RenderTarget[0].RenderTargetWriteMask = 0xfu;

		D3D12_DEPTH_STENCIL_DESC dd;
		dd.DepthEnable = true;
		dd.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		dd.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		dd.StencilEnable = false;

		Egg::Mesh::Material::P fireMaterial = Egg::Mesh::Material::Create();
		fireMaterial->SetRootSignature(billboardRootSig);
		fireMaterial->SetVertexShader(billboardVertexShader);
		fireMaterial->SetGeometryShader(billboardGeometryShader);
		fireMaterial->SetPixelShader(firePixelShader);
		fireMaterial->SetDepthStencilState(dd);
		fireMaterial->SetBlendState(transparency);
		fireMaterial->SetDSVFormat(DXGI_FORMAT_D32_FLOAT);
		fireMaterial->SetConstantBuffer(perObjectCb);
		fireMaterial->SetConstantBuffer(perFrameCb);
		fireMaterial->SetSrvHeap(2, particleSrvHeap);

		fireBillboardSet = Egg::Mesh::Shaded::Create(
			psoManager, fireMaterial, particlesGeometry);

		shadowMapVertexShader = Egg::Shader::LoadCso("Shaders/shadowMapVS.cso");
		shadowMapGeometryShader = Egg::Shader::LoadCso("Shaders/shadowMapGS.cso");
		shadowMapPixelShader = Egg::Shader::LoadCso("Shaders/shadowMapPS.cso");
		shadowMapRootSig = Egg::Shader::LoadRootSignature(device.Get(), shadowMapVertexShader.Get());

		shadowedVertexShader = Egg::Shader::LoadCso("Shaders/shadowedVS.cso");
		shadowedPixelShader = Egg::Shader::LoadCso("Shaders/shadowedPS.cso");
		shadowedRootSig = Egg::Shader::LoadRootSignature(device.Get(), shadowedVertexShader.Get());

		RunScript("scene.lua");

		UploadResources();
	}


	virtual void AddDefaultShadedMeshes(
		Egg::Mesh::Flip::P flipMesh,
		Egg::Mesh::IndexedGeometry::P indexedGeometry,
		std::string texturePath
	) override {	
		Egg::Texture2D tex = LoadTexture2D(texturePath);
		tex.CreateSRV(device.Get(), srvHeap.Get(), srvCount);

		Egg::TextureCube env = LoadTextureCube(envTexturePath);
		env.CreateSRV(device.Get(), srvHeap.Get(), srvCount + 1);

		Egg::Mesh::Material::P material = Egg::Mesh::Material::Create();
		material->SetRootSignature(shadowedRootSig);
		material->SetVertexShader(shadowedVertexShader);
		material->SetPixelShader(shadowedPixelShader);
		material->SetDepthStencilState(CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT));
		material->SetDSVFormat(DXGI_FORMAT_D32_FLOAT);
		material->SetSrvHeap(2, srvHeap,
			srvCount * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
		material->SetConstantBuffer(perObjectCb, sizeof(Egg::Scene::PerObjectData));
		material->SetConstantBuffer(perFrameCb);
		srvCount += 2;

		//D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR

		auto shadedMesh = Egg::Mesh::Shaded::Create(psoManager, material, indexedGeometry);
		flipMesh->Add(0, shadedMesh);

		Egg::Mesh::Material::P shadowMapMaterial = Egg::Mesh::Material::Create();
		shadowMapMaterial->SetRootSignature(shadowMapRootSig);
		shadowMapMaterial->SetVertexShader(shadowMapVertexShader);
		shadowMapMaterial->SetGeometryShader(shadowMapGeometryShader);
		shadowMapMaterial->SetPixelShader(shadowMapPixelShader);
		shadowMapMaterial->SetDepthStencilState(CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT));
		shadowMapMaterial->SetDSVFormat(DXGI_FORMAT_D32_FLOAT);
		shadowMapMaterial->SetSrvHeap(2, srvHeap,
			srvCount * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
		shadowMapMaterial->SetConstantBuffer(perObjectCb, sizeof(Egg::Scene::PerObjectData));
		shadowMapMaterial->SetConstantBuffer(perFrameCb);
		srvCount += 2;

		auto shadowShadedMesh = Egg::Mesh::Shaded::Create(psoManager, shadowMapMaterial, indexedGeometry);
		flipMesh->Add(1, shadowShadedMesh);
	}

	virtual void Update(float dt, float T) override {
		using namespace Egg::Math;
		perFrameCb->lightViewProjTransform = 
			Float4x4::View(Float3(0, 20, 0), Float3(0, -1, 0), Float3(1, 0, 0)) *
			Float4x4::Proj(1.0, 1.0, 0.1, 100.0);
		perFrameCb->lightViewProjTransform2 =
			Float4x4::View(Float3(0, 20, 0.5), Float3(0, -1, 0), Float3(1, 0, 0)) *
			Float4x4::Proj(1.0, 1.0, 0.1, 100.0);
		perFrameCb->billboardSize = Float4(50.1, 50.1, 0, 0) 
			* cameras[currentCameraIndex]->GetProjMatrix();
		for (int i = 0; i < particles.size(); i++)
			particles.at(i).move(dt);

		using namespace Egg::Math;
		struct CameraDepthComparator {
			Float3 ahead;
			bool operator()(const Particle& a,
				const Particle& b) {
				return
					a.position.Dot(ahead) >
					b.position.Dot(ahead) + 0.01;
			}
		} comp = { cameras[currentCameraIndex]->GetAhead() };
		std::sort(particles.begin(), particles.end(), comp);


		particlesGeometry->SetData(particles.data(), particles.size() * sizeof(particles[0]));

		__super::Update(dt, T);
	}

};


