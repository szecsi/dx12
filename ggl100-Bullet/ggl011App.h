#pragma once

#include <Egg/Scene/ManagerApp.h>
#include <Egg/Importer.h>
#include <Egg/Math/Math.h>
#include <Egg/Mesh/Prefabs.h>
#include <Egg/Mesh/Multi.h>
#include <Egg/ConstantBuffer.hpp>

#include <Egg/Cam/FirstPerson.h>
#include <Egg/Scene/StaticEntity.h>
#include "ConstantBufferTypes.h"
#include "Particle.h"
#include <vector>
#include <algorithm>

#include <Egg/Scene/PerObjectData.h>

using namespace Egg::Math;

class ggl012App : public Egg::Scene::ManagerApp {
protected:
public:
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

		__super::PopulateCommandList();

		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

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

	virtual void CreateResources() override {
		Egg::Scene::ManagerApp::CreateResources();
	}

	virtual void ReleaseResources() override {
		Egg::Scene::ManagerApp::ReleaseResources();
	}

	virtual void LoadAssets() override {
		using namespace Egg::Scene;
		using namespace Egg::Mesh;

		__super::LoadAssets();

//		Multi::P podMesh = ManagerApp::LoadMultiMesh("geopod.x", 0, "pod");
		Multi::P podMesh = ManagerApp::LoadMultiMesh("giraffe.obj", 0, "giraffe");


		for (int i = 0; i < 100; i++) {
			auto e = Egg::Scene::StaticEntity::Create(podMesh);
			e->Translate(Float3(i, 0, 0));
			ManagerApp::AddEntity(e);
		}

		UploadResources();
	}


};


