#include "stdafx.h"
#include "App.h"
#include "ThrowOnFail.h"

using namespace Egg11;

HRESULT App::createSwapChainResources() {
	using namespace Microsoft::WRL;

	Egg11::ThrowOnFail("Failed to get swapchain backbuffer.", __FILE__, __LINE__) ^
		swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()));

	Egg11::ThrowOnFail("Failed to create render target view.", __FILE__, __LINE__) ^
		device->CreateRenderTargetView(
			backBuffer.Get(),
			nullptr,
			defaultRenderTargetView.GetAddressOf()
			);

	// Create a depth stencil view for use with 3D rendering if needed.
	CD3D11_TEXTURE2D_DESC depthStencilDesc(
		DXGI_FORMAT_D24_UNORM_S8_UINT,
		static_cast<UINT>(swapChainDesc.Width),
		static_cast<UINT>(swapChainDesc.Height),
		1, // This depth stencil view has only one texture.
		1, // Use a single mipmap level.
		D3D11_BIND_DEPTH_STENCIL
		);

	Egg11::ThrowOnFail("Failed to create depth stencil texture.", __FILE__, __LINE__) ^
		device->CreateTexture2D(
			&depthStencilDesc,
			nullptr,
			&depthStencil
			);

	CD3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc(D3D11_DSV_DIMENSION_TEXTURE2D);
	Egg11::ThrowOnFail("Failed to create depth stencil view.", __FILE__, __LINE__) ^
		device->CreateDepthStencilView(
			depthStencil.Get(),
			&depthStencilViewDesc,
			defaultDepthStencilView.GetAddressOf()
			);

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
	backBuffer.Reset();
	defaultRenderTargetView.Reset();
	depthStencil.Reset();
	defaultDepthStencilView.Reset();
	return S_OK;
}
