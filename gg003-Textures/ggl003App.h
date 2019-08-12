#pragma once

#include <Egg/SimpleApp.h>
#include <Egg/Importer.h>
#include <Egg/Math/Math.h>
#include <Egg/ConstantBuffer.hpp>

#include "ConstantBufferTypes.h"

using namespace Egg::Math;

class ggl003App : public Egg::SimpleApp {
protected:
	Egg::Mesh::Shaded::P shadedMesh;
	Float4x4 rotation;
	Egg::ConstantBuffer<PerObjectCb> cb;
	com_ptr<ID3D12DescriptorHeap> testHeap;
	Egg::Texture tex;
	bool b = false;
	bool c = false;
public:
	virtual void Update(float dt, float T) override {
		rotation *= Float4x4::Rotation(Float3::UnitY, dt);
		cb->modelTransform = Float4x4::Translation(Float3{ 0.0f, 0.0f, -0.5f }) * rotation * Float4x4::Translation(Float3{ 0.0f, 0.0f, 0.5f });
		cb.Upload();
	}

	virtual void PopulateCommandList() override {
		commandAllocator->Reset();
		commandList->Reset(commandAllocator.Get(), nullptr);

		commandList->RSSetViewports(1, &viewPort);
		commandList->RSSetScissorRects(1, &scissorRect);

		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

		CD3DX12_CPU_DESCRIPTOR_HANDLE rHandle(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), frameIndex, rtvDescriptorHandleIncrementSize);
		CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(dsvHeap->GetCPUDescriptorHandleForHeapStart());
		commandList->OMSetRenderTargets(1, &rHandle, FALSE, &dsvHandle);

		const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
		commandList->ClearRenderTargetView(rHandle, clearColor, 0, nullptr);
		commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		ID3D12DescriptorHeap* descriptorHeaps[] = { testHeap.Get() };
		commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		shadedMesh->SetPipelineState(commandList.Get());
		shadedMesh->BindConstantBuffer(commandList.Get(), cb);
		commandList->SetGraphicsRootDescriptorTable(1, testHeap->GetGPUDescriptorHandleForHeapStart());
		shadedMesh->Draw(commandList.Get());

		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

		DX_API("Failed to close command list")
			commandList->Close();
	}

	/*
	Almost a render call
	*/
	void UploadResources() {
		DX_API("Failed to reset command allocator (UploadResources)")
			commandAllocator->Reset();
		DX_API("Failed to reset command list (UploadResources)")
			commandList->Reset(commandAllocator.Get(), nullptr);

		tex.UploadResource(commandList.Get());

		DX_API("Failed to close command list (UploadResources)")
			commandList->Close();

		ID3D12CommandList * commandLists[] = { commandList.Get() };
		commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

		WaitForPreviousFrame();
	}


	virtual void LoadAssets() override {
		cb.CreateResources(device.Get());

		com_ptr<ID3DBlob> vertexShader = Egg::Shader::LoadCso("Shaders/cbBasicVS.cso");
		com_ptr<ID3DBlob> pixelShader = Egg::Shader::LoadCso("Shaders/DefaultPS.cso");
		com_ptr<ID3D12RootSignature> rootSig = Egg::Shader::LoadRootSignature(device.Get(), vertexShader.Get());

		Egg::Mesh::Material::P material = Egg::Mesh::Material::Create();
		material->SetRootSignature(rootSig);
		material->SetVertexShader(vertexShader);
		material->SetPixelShader(pixelShader);
		material->SetDepthStencilState(CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT));
		material->SetDSVFormat(DXGI_FORMAT_D32_FLOAT);

		Egg::Mesh::Geometry::P geometry = Egg::Importer::ImportSimpleObj(device.Get(), "giraffe.obj");

		tex = Egg::Importer::ImportTexture(device.Get(), "giraffe.jpg");

		shadedMesh = Egg::Mesh::Shaded::Create(psoManager.get(), material, geometry);

		D3D12_DESCRIPTOR_HEAP_DESC dhd;
		dhd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		dhd.NodeMask = 0;
		dhd.NumDescriptors = 1;
		dhd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

		DX_API("Failed to create descriptor heap for texture")
			device->CreateDescriptorHeap(&dhd, IID_PPV_ARGS(testHeap.GetAddressOf()));

		D3D12_SHADER_RESOURCE_VIEW_DESC srvd;
		ZeroMemory(&srvd, sizeof(D3D12_SHADER_RESOURCE_VIEW_DESC));
		srvd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvd.Texture2D.MipLevels = 1;

		device->CreateShaderResourceView(tex.operator->(), &srvd, testHeap->GetCPUDescriptorHandleForHeapStart());

		UploadResources();
	}

};


