#pragma once
#include "Egg/Common.h"
#include <Egg/OpenXR/OpenXRApp.h>
#include <Egg/Shader.h>
#include <Egg/Math/Float4x4.h>
#include <string>
#include <vector>

// Minimal proof-of-pipeline VR sample: a single slowly-spinning, per-face-
// colored cube, 1.5m in front of the headset's initial position. No
// textures, no scene manager, no Lua -- just enough content to tell at a
// glance whether OpenXRApp's device/session/swapchain/per-eye render loop
// is actually working on real hardware (correct stereo separation, correct
// head tracking, no validation errors).
class VrTestApp : public Egg::OpenXRApp {
protected:
	struct ColorVertex {
		Egg::Math::float3 position;
		Egg::Math::float3 color;
	};

	com_ptr<ID3D12RootSignature> rootSig;
	com_ptr<ID3D12PipelineState> pso;
	com_ptr<ID3D12Resource> vertexBuffer;
	com_ptr<ID3D12Resource> indexBuffer;
	D3D12_VERTEX_BUFFER_VIEW vbv{};
	D3D12_INDEX_BUFFER_VIEW ibv{};
	UINT indexCount = 0;

	float rotationAngle = 0.0f;

	com_ptr<ID3D12Resource> CreateUploadBuffer(const void* data, size_t sizeBytes) {
		com_ptr<ID3D12Resource> res;
		DX_API("Failed to create upload buffer")
			device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(sizeBytes),
				D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(res.GetAddressOf()));

		void* mapped = nullptr;
		CD3DX12_RANGE readRange(0, 0);
		DX_API("Failed to map upload buffer")
			res->Map(0, &readRange, &mapped);
		memcpy(mapped, data, sizeBytes);
		res->Unmap(0, nullptr);
		return res;
	}

public:
	virtual void Update(float dt, float T) override {
		rotationAngle = T * 0.5f; // slow spin, radians/sec
	}

	virtual void LoadAssets() override {
		using namespace Egg::Math;

		com_ptr<ID3DBlob> vs = Egg::Shader::LoadCso("Shaders/vrTestVS.cso");
		com_ptr<ID3DBlob> ps = Egg::Shader::LoadCso("Shaders/vrTestPS.cso");
		rootSig = Egg::Shader::LoadRootSignature(device.Get(), vs.Get());

		D3D12_INPUT_ELEMENT_DESC layout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = rootSig.Get();
		psoDesc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
		psoDesc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // avoid winding-order guesswork for a first test
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = TRUE;
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.InputLayout = { layout, _countof(layout) };
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // must match CreateXrSwapchains()'s format
		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT; // must match CreateSwapChainResources()'s eye depth buffers
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.SampleDesc.Count = 1;
		{
			HRESULT hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(pso.GetAddressOf()));
			if (FAILED(hr)) {
				// The generic HRESULT ("the parameter is incorrect") doesn't say
				// WHICH field -- the D3D12 debug layer (enabled in main.cpp)
				// knows exactly, but normally only reports it via
				// OutputDebugString/a debugger. Pull it directly instead so it
				// shows up in the assert dialog even without one attached.
				std::string details;
				com_ptr<ID3D12InfoQueue> infoQueue;
				if (SUCCEEDED(device.As(&infoQueue))) {
					UINT64 n = infoQueue->GetNumStoredMessages();
					for (UINT64 i = 0; i < n; i++) {
						SIZE_T len = 0;
						infoQueue->GetMessage(i, nullptr, &len);
						std::vector<char> buf(len);
						D3D12_MESSAGE* msg = reinterpret_cast<D3D12_MESSAGE*>(buf.data());
						infoQueue->GetMessage(i, msg, &len);
						details += msg->pDescription;
						details += "\n";
					}
				}
				ASSERT(false, "Failed to create vrTest PSO (HR=0x%08X). Debug layer messages:\n%s", hr, details.c_str());
			}
		}

		// 0.4m cube, one flat color per face, centered at the local origin.
		const float e = 0.2f;
		ColorVertex verts[] = {
			{{ e,-e,-e}, {1,0,0}}, {{ e, e,-e}, {1,0,0}}, {{ e, e, e}, {1,0,0}}, {{ e,-e, e}, {1,0,0}}, // +X red
			{{-e,-e, e}, {0,1,1}}, {{-e, e, e}, {0,1,1}}, {{-e, e,-e}, {0,1,1}}, {{-e,-e,-e}, {0,1,1}}, // -X cyan
			{{-e, e,-e}, {0,1,0}}, {{-e, e, e}, {0,1,0}}, {{ e, e, e}, {0,1,0}}, {{ e, e,-e}, {0,1,0}}, // +Y green
			{{-e,-e, e}, {1,0,1}}, {{-e,-e,-e}, {1,0,1}}, {{ e,-e,-e}, {1,0,1}}, {{ e,-e, e}, {1,0,1}}, // -Y magenta
			{{-e,-e, e}, {0,0,1}}, {{ e,-e, e}, {0,0,1}}, {{ e, e, e}, {0,0,1}}, {{-e, e, e}, {0,0,1}}, // +Z blue
			{{ e,-e,-e}, {1,1,0}}, {{-e,-e,-e}, {1,1,0}}, {{-e, e,-e}, {1,1,0}}, {{ e, e,-e}, {1,1,0}}, // -Z yellow
		};
		uint16_t indices[] = {
			 0, 1, 2,  0, 2, 3,
			 4, 5, 6,  4, 6, 7,
			 8, 9,10,  8,10,11,
			12,13,14, 12,14,15,
			16,17,18, 16,18,19,
			20,21,22, 20,22,23,
		};
		indexCount = _countof(indices);

		vertexBuffer = CreateUploadBuffer(verts, sizeof(verts));
		vbv.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
		vbv.SizeInBytes = sizeof(verts);
		vbv.StrideInBytes = sizeof(ColorVertex);

		indexBuffer = CreateUploadBuffer(indices, sizeof(indices));
		ibv.BufferLocation = indexBuffer->GetGPUVirtualAddress();
		ibv.SizeInBytes = sizeof(indices);
		ibv.Format = DXGI_FORMAT_R16_UINT;
	}

	virtual void ReleaseAssets() override {
		vertexBuffer.Reset();
		indexBuffer.Reset();
		pso.Reset();
		rootSig.Reset();
	}

	// Called once per eye per frame by OpenXRApp::Render() with an
	// already-open command list -- do not Reset()/Close() it here.
	virtual void PopulateEyeCommandList() override {
		using namespace Egg::Math;

		D3D12_CPU_DESCRIPTOR_HANDLE rtv = GetEyeRtv(xrCurrentEye, xrCurrentImageIndex[xrCurrentEye]);
		D3D12_CPU_DESCRIPTOR_HANDLE dsv = GetEyeDsv(xrCurrentEye);

		commandList->RSSetViewports(1, &eyeViewports[xrCurrentEye]);
		commandList->RSSetScissorRects(1, &eyeScissorRects[xrCurrentEye]);
		commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

		// Pure black, not near-black: written to an sRGB render target, a
		// low-but-nonzero linear value like 0.05 gets perceptually
		// brightened a lot on display (sRGB's encoding curve is steep near
		// zero) -- easy to mistake for "everything is grey" on its own.
		const float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
		commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
		commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		// 1.5m in front of the local reference space's origin (the
		// headset's pose when the session started).
		float4x4 world = float4x4::Rotation(float3::UnitY, rotationAngle) * float4x4::Translation(float3(0.0f, 0.0f, -1.5f));
		float4x4 wvp = world * eyeViewMatrix[xrCurrentEye] * eyeProjMatrix[xrCurrentEye];

		commandList->SetGraphicsRootSignature(rootSig.Get());
		commandList->SetGraphicsRoot32BitConstants(0, 16, wvp.l, 0);
		commandList->SetPipelineState(pso.Get());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->IASetVertexBuffers(0, 1, &vbv);
		commandList->IASetIndexBuffer(&ibv);
		commandList->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);
	}
};
