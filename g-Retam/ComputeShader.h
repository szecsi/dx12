#pragma once
#include "Egg/Common.h"

class ComputeShader {
public:
	com_ptr<ID3D12PipelineState> pso;
	com_ptr<ID3D12RootSignature> rootSig;

public:
	void createResources(com_ptr<ID3D12Device> device, std::string csoName) {
		com_ptr<ID3DBlob> computeShader = Egg::Shader::LoadCso(csoName);
		rootSig = Egg::Shader::LoadRootSignature(device.Get(), computeShader.Get());

		D3D12_COMPUTE_PIPELINE_STATE_DESC descComputePSO = {};
		descComputePSO.pRootSignature = rootSig.Get();
		descComputePSO.CS.pShaderBytecode = computeShader->GetBufferPointer();
		descComputePSO.CS.BytecodeLength = computeShader->GetBufferSize();

		DX_API("Failed to create compute pipeline state object.")
			device->CreateComputePipelineState(&descComputePSO, IID_PPV_ARGS(pso.ReleaseAndGetAddressOf()));
		pso->SetName(L"Compute PSO" /* + csoName*/);
	}

	void setup(com_ptr<ID3D12GraphicsCommandList> computeCommandList, D3D12_GPU_DESCRIPTOR_HANDLE uavHandle) {
		computeCommandList->SetComputeRootSignature(rootSig.Get());
		computeCommandList->SetComputeRootDescriptorTable(1, uavHandle);
		computeCommandList->SetPipelineState(pso.Get());
	}
};