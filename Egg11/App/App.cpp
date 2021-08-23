#include "stdafx.h"
#include "App.h"
#include "ThrowOnFail.h"

using namespace Egg11;

HRESULT App::createSwapChainResources() {
	using namespace Microsoft::WRL;
	return S_OK;
};

Microsoft::WRL::ComPtr<ID3DBlob> App::loadShaderCode(const std::string& shaderFilename) {
	std::fstream f(systemEnvironment.resolveShaderPath(shaderFilename), std::ios::in | std::ios::binary);
	if (f.bad())
	{
		return nullptr;
	}
	auto begin = f.tellg();
	f.seekg(0, std::ios::end);
	auto end = f.tellg();
	f.seekg(0);
	ID3DBlob* shaderCode;
	D3DCreateBlob(end - begin, &shaderCode);
	f.read((char*)shaderCode->GetBufferPointer(), shaderCode->GetBufferSize());
	f.close();
	return shaderCode;
}


HRESULT App::releaseSwapChainResources()
{
	return S_OK;
}
