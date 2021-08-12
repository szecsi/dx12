#include "stdafx.h"
#include "Mesh/Shader.h"

using namespace Egg11;

Mesh::Shader::Shader(
	std::string filename,
	Microsoft::WRL::ComPtr<ID3D11Device> device,
	Microsoft::WRL::ComPtr<ID3DBlob> byteCode)
	: filename(filename), byteCode(byteCode)
{
	D3DReflect(byteCode->GetBufferPointer(), byteCode->GetBufferSize(),
		IID_ID3D11ShaderReflection, (void**)reflection.GetAddressOf());
	
	D3D11_SHADER_DESC shaderDesc;
	reflection->GetDesc(&shaderDesc);

	if (D3D11_SHVER_GET_TYPE(shaderDesc.Version) == D3D11_SHVER_VERTEX_SHADER) {
		Microsoft::WRL::ComPtr<ID3D11VertexShader> vs;
		device->CreateVertexShader(byteCode->GetBufferPointer(), byteCode->GetBufferSize(), nullptr, vs.GetAddressOf());
		shader = vs;
	} else if (D3D11_SHVER_GET_TYPE(shaderDesc.Version) == D3D11_SHVER_PIXEL_SHADER) {
		Microsoft::WRL::ComPtr<ID3D11PixelShader> ps;
		device->CreatePixelShader(byteCode->GetBufferPointer(), byteCode->GetBufferSize(), nullptr, ps.GetAddressOf());
		shader = ps;
	}
	else if (D3D11_SHVER_GET_TYPE(shaderDesc.Version) == D3D11_SHVER_GEOMETRY_SHADER) {
		Microsoft::WRL::ComPtr<ID3D11GeometryShader> gs;
		device->CreateGeometryShader(byteCode->GetBufferPointer(), byteCode->GetBufferSize(), nullptr, gs.GetAddressOf());
		shader = gs;
	}
	else if (D3D11_SHVER_GET_TYPE(shaderDesc.Version) == D3D11_SHVER_COMPUTE_SHADER) {
		Microsoft::WRL::ComPtr<ID3D11ComputeShader> cs;
		device->CreateComputeShader(byteCode->GetBufferPointer(), byteCode->GetBufferSize(), nullptr, cs.GetAddressOf());
		shader = cs;
	}
}

uint Mesh::Shader::getResourceIndex(std::string name)
{
	D3D11_SHADER_INPUT_BIND_DESC bindDesc;
	if (S_OK == reflection->GetResourceBindingDescByName(name.c_str(), &bindDesc))
		return bindDesc.BindPoint;
	else
		return -1;
}