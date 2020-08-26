#pragma once

#include <Egg/SimpleApp.h>
#include <Egg/Importer.h>
#include <Egg/Math/Math.h>
#include <Egg/ConstantBuffer.hpp>
#include <Egg/Mesh/Prefabs.h>

#include "ConstantBufferTypes.h"

using namespace Egg::Math;

class ggl007App : public Egg::SimpleApp {
protected:
	Egg::Mesh::Shaded::P envMapMesh;
	Egg::Mesh::Shaded::P shadedMesh;
	Float4x4 rotation;
	Egg::ConstantBuffer<PerObjectCb> cb;
	Egg::ConstantBuffer<PerObjectCb> parallaxCb;
	Egg::ConstantBuffer<PerFrameCb> perFrameCb;
	com_ptr<ID3D12DescriptorHeap> srvHeap;

	Egg::Texture2D giraffeTex;

	Egg::Texture2D diffuseTex;
	Egg::Texture2D normalTex;
	Egg::Texture2D bumpTex;

	Egg::TextureCube envTex;

	Egg::Mesh::Shaded::P parallax;
public:
	virtual void Update(float dt, float T) override {
		rotation =  Float4x4::Rotation(Float3::UnitX, 1.58f) * Float4x4::Rotation(Float3::UnitY, 0.5f * T);
		cb->model = rotation * Float4x4::Translation(Float3{ 0.0f, 0.0f, 0.0f });
		cb->invModel = cb->model.Invert();
		cb.Upload();

		parallaxCb->model = rotation * Float4x4::Translation(Float3{ 100.0f, 1.25f, 0.0f });
		parallaxCb->invModel = parallaxCb->model.Invert();
		parallaxCb.Upload();

		perFrameCb->eyePos = Float4{ 0, 0.0f, -6.0f, 1.0f };
		perFrameCb->viewProj = Float4x4::View(perFrameCb->eyePos.xyz, Float3::UnitZ, Float3::UnitY) *  Float4x4::Proj(0.9f, aspectRatio, 1.0f, 100.0f);
		perFrameCb->lightPos = Float4{ 0, 1, 0, 0 };
		perFrameCb->lightIntensity = Float4::One;
		perFrameCb->invViewProj = (Float4x4::View(Float3::Zero, Float3::UnitZ, Float3::UnitY) * Float4x4::Proj(0.9f, aspectRatio, 1.0f, 100.0f)).Invert();
		perFrameCb.Upload();
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

		ID3D12DescriptorHeap* descriptorHeaps[] = { srvHeap.Get() };
		commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		UINT incrementSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		CD3DX12_GPU_DESCRIPTOR_HANDLE{ srvHeap->GetGPUDescriptorHandleForHeapStart(), 0, incrementSize };

		D3D12_GPU_DESCRIPTOR_HANDLE texHandle = srvHeap->GetGPUDescriptorHandleForHeapStart();
		texHandle.ptr += incrementSize;

		shadedMesh->SetPipelineState(commandList.Get());
		shadedMesh->BindConstantBuffer(commandList.Get(), cb);
		shadedMesh->BindConstantBuffer(commandList.Get(), perFrameCb);
		commandList->SetGraphicsRootDescriptorTable(2, srvHeap->GetGPUDescriptorHandleForHeapStart());
		shadedMesh->Draw(commandList.Get());

		envMapMesh->SetPipelineState(commandList.Get());
		envMapMesh->BindConstantBuffer(commandList.Get(), perFrameCb);
		commandList->SetGraphicsRootDescriptorTable(2, srvHeap->GetGPUDescriptorHandleForHeapStart());
		envMapMesh->Draw(commandList.Get());

		
		parallax->SetPipelineState(commandList.Get());
		parallax->BindConstantBuffer(commandList.Get(), parallaxCb);
		parallax->Draw(commandList.Get());

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

		diffuseTex.UploadResource(commandList.Get());
		normalTex.UploadResource(commandList.Get());
		bumpTex.UploadResource(commandList.Get());
		envTex.UploadResource(commandList.Get());

		DX_API("Failed to close command list (UploadResources)")
			commandList->Close();

		ID3D12CommandList * commandLists[] = { commandList.Get() };
		commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

		WaitForPreviousFrame();

		diffuseTex.ReleaseUploadResources();
		normalTex.ReleaseUploadResources();
		bumpTex.ReleaseUploadResources();
		envTex.ReleaseUploadResources();
	}

	virtual void CreateResources() override {
		Egg::SimpleApp::CreateResources();

		D3D12_DESCRIPTOR_HEAP_DESC dhd;
		dhd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		dhd.NodeMask = 0;
		dhd.NumDescriptors = 4;
		dhd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

		DX_API("Failed to create descriptor heap for texture")
			device->CreateDescriptorHeap(&dhd, IID_PPV_ARGS(srvHeap.GetAddressOf()));
	}

	virtual void ReleaseResources() override {
		srvHeap.Reset();
		cb.ReleaseResources();
		perFrameCb.ReleaseResources();
		parallaxCb.ReleaseResources();

		Egg::SimpleApp::ReleaseResources();
	}

	virtual void LoadAssets() override {
		cb.CreateResources(device.Get());
		perFrameCb.CreateResources(device.Get());
		parallaxCb.CreateResources(device.Get());

		com_ptr<ID3DBlob> vertexShader = Egg::Shader::LoadCso("Shaders/EnvNormalMapVS.cso");
		com_ptr<ID3DBlob> pixelShader = Egg::Shader::LoadCso("Shaders/EnvNormalMapPS.cso");
		com_ptr<ID3D12RootSignature> rootSig = Egg::Shader::LoadRootSignature(device.Get(), vertexShader.Get());

		Egg::Mesh::Material::P material = Egg::Mesh::Material::Create();
		material->SetRootSignature(rootSig);
		material->SetVertexShader(vertexShader);
		material->SetPixelShader(pixelShader);
		material->SetDepthStencilState(CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT));
		material->SetDSVFormat(DXGI_FORMAT_D32_FLOAT);

		com_ptr<ID3DBlob> parallaxVS = Egg::Shader::LoadCso("Shaders/ParallaxVS.cso");
		com_ptr<ID3DBlob> parallaxPS = Egg::Shader::LoadCso("Shaders/ParallaxPS.cso");

		Egg::Mesh::Material::P parallaxMat = Egg::Mesh::Material::Create();
		parallaxMat->SetRootSignature(rootSig);
		parallaxMat->SetVertexShader(parallaxVS);
		parallaxMat->SetPixelShader(parallaxPS);
		parallaxMat->SetDepthStencilState(CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT));
		parallaxMat->SetDSVFormat(DXGI_FORMAT_D32_FLOAT);

		com_ptr<ID3DBlob> envMapVS = Egg::Shader::LoadCso("Shaders/EnvMapVS.cso");
		com_ptr<ID3DBlob> envMapPS = Egg::Shader::LoadCso("Shaders/EnvMapPS.cso");
		com_ptr<ID3D12RootSignature> envMapRS = Egg::Shader::LoadRootSignature(device.Get(), envMapVS.Get());

		Egg::Mesh::Material::P envMapMat = Egg::Mesh::Material::Create();
		envMapMat->SetRootSignature(envMapRS);
		envMapMat->SetVertexShader(envMapVS);
		envMapMat->SetPixelShader(envMapPS);
		envMapMat->SetDepthStencilState(CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT));
		envMapMat->SetDSVFormat(DXGI_FORMAT_D32_FLOAT);

		Egg::Mesh::Geometry::P fsQuad = Egg::Mesh::Prefabs::IndexedFullscreenQuad(device.Get());

		Egg::Mesh::Geometry::P bumpGeom =// Egg::Mesh::Prefabs::UnitBox(device.Get());
			
			Egg::Importer::ImportWithTangentSpace(device.Get(), "sphere.fbx");

		envMapMesh = Egg::Mesh::Shaded::Create(psoManager.get(), envMapMat, fsQuad);

		parallax = Egg::Mesh::Shaded::Create(psoManager.get(), parallaxMat, bumpGeom);

		shadedMesh = Egg::Mesh::Shaded::Create(psoManager.get(), material, bumpGeom);

		diffuseTex = Egg::Importer::ImportTexture2D(device.Get(), "rusty.png");
		diffuseTex.CreateSRV(device.Get(), srvHeap.Get(), 0);
		
		normalTex = Egg::Importer::ImportTexture2D(device.Get(), "rusted_normal.jpg");
		normalTex.CreateSRV(device.Get(), srvHeap.Get(), 1);

		bumpTex = Egg::Importer::ImportTexture2D(device.Get(), "brick3_disp.jpg");
		bumpTex.CreateSRV(device.Get(), srvHeap.Get(), 2);

		envTex = Egg::Importer::ImportTextureCube(device.Get(), "cloudynoon.dds");
		envTex.CreateSRV(device.Get(), srvHeap.Get(), 3);

		UploadResources();
	}

};


