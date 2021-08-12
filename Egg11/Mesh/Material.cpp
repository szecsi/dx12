#include "stdafx.h"
#include "Mesh/Material.h"
#include "ThrowOnFail.h"

using namespace Egg11;

Mesh::Material::Material()
{}

Mesh::Material::~Material()
{
}

void Mesh::Material::setShader(ShaderStageFlag stage, Shader::P shader) {
	shaders[stage] = shader;
}

void Mesh::Material::apply(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
{
	context->GSSetShader(nullptr, nullptr, 0);
	context->RSSetState(rasterizerState.Get());
	context->OMSetDepthStencilState(depthStencilState.Get(), 0);
	float ones[] = { 1, 1, 1, 1 };
	context->OMSetBlendState(blendState.Get(), ones, 0xff);

	ID3D11Buffer* buffers[64];
	ID3D11SamplerState* samplers[64];
	ID3D11ShaderResourceView* srvs[64];
	for (auto iShader : shaders) {
		//copy to array
		uint cCbs = 0;
		for (auto& cb : cbs[iShader.first])
			buffers[cCbs++] = cb.Get();
		uint cSrs = 0;
		for (auto& sr : srs[iShader.first])
			srvs[cSrs++] = sr.Get();
		uint cSss = 0;
		for (auto& ss : sss[iShader.first])
			samplers[cSss++] = ss.Get();

		switch (iShader.first) {
		case ShaderStageFlag::Vertex:
			context->VSSetShader(iShader.second->as<ID3D11VertexShader>().Get(), nullptr, 0);
			if(cCbs) context->VSSetConstantBuffers(0, cCbs, buffers);
			if(cSss) context->VSSetSamplers(0, cSss, samplers);
			if(cSrs) context->VSSetShaderResources(0, cSrs, srvs);
			break;
		case ShaderStageFlag::Hull:
			context->HSSetShader(iShader.second->as<ID3D11HullShader>().Get(), nullptr, 0);
			if (cCbs) context->HSSetConstantBuffers(0, cCbs, buffers);
			if (cSss) context->HSSetSamplers(0, cSss, samplers);
			if (cSrs) context->HSSetShaderResources(0, cSrs, srvs);
			break;
		case ShaderStageFlag::Domain:
			context->DSSetShader(iShader.second->as<ID3D11DomainShader>().Get(), nullptr, 0);
			if (cCbs) context->DSSetConstantBuffers(0, cCbs, buffers);
			if (cSss) context->DSSetSamplers(0, cSss, samplers);
			if (cSrs) context->DSSetShaderResources(0, cSrs, srvs);
			break;
		case ShaderStageFlag::Geometry:
			context->GSSetShader(iShader.second->as<ID3D11GeometryShader>().Get(), nullptr, 0);
			if (cCbs) context->GSSetConstantBuffers(0, cCbs, buffers);
			if (cSss) context->GSSetSamplers(0, cSss, samplers);
			if (cSrs) context->GSSetShaderResources(0, cSrs, srvs);
			break;
		case ShaderStageFlag::Pixel:
			context->PSSetShader(iShader.second->as<ID3D11PixelShader>().Get(), nullptr, 0);
			if (cCbs) context->PSSetConstantBuffers(0, cCbs, buffers);
			if (cSss) context->PSSetSamplers(0, cSss, samplers);
			if (cSrs) context->PSSetShaderResources(0, cSrs, srvs);
			break;
		case ShaderStageFlag::Compute:
			context->CSSetShader(iShader.second->as<ID3D11ComputeShader>().Get(), nullptr, 0);
			if (cCbs) context->CSSetConstantBuffers(0, cCbs, buffers);
			if (cSss) context->CSSetSamplers(0, cSss, samplers);
			if (cSrs) context->CSSetShaderResources(0, cSrs, srvs);
			break;
		};
	};

}

void Mesh::Material::setCb(std::string name, Microsoft::WRL::ComPtr<ID3D11Buffer> cb, ShaderStageFlag stage) {
	bool found = false;
	for (auto iShader : shaders) {
		uint index = iShader.second->getResourceIndex(name);
		if (index != -1) {
			setCb(iShader.first, index, cb);
			found = true;
		}
	}
	if (!found)
		OutputDebugStringA((name + " constant buffer has not been found in any shaders.").c_str());
}

void Mesh::Material::setSamplerState(std::string name, Microsoft::WRL::ComPtr<ID3D11SamplerState> ss, ShaderStageFlag stage)
{
	bool found = false;
	for (auto iShader : shaders) {
		uint index = iShader.second->getResourceIndex(name);
		if (index != -1) {
			setSamplerState(iShader.first, index, ss);
			found = true;
		}
	}
	if (!found)
		OutputDebugStringA((name + " sampler state has not been found in any shaders.").c_str());
}

void Mesh::Material::setShaderResource(std::string name, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> sr, ShaderStageFlag stage)
{
	bool found = false;
	for (auto iShader : shaders) {
		uint index = iShader.second->getResourceIndex(name);
		if (index != -1) {
			setShaderResource(iShader.first, index, sr);
			found = true;
		}
	}
	if (!found)
		OutputDebugStringA((name + " shader resource has not been found in any shaders.").c_str());
}
