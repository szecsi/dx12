#pragma once

#include "SystemEnvironment.h"

namespace Egg11 {
	GG_CLASS(App)
	protected:
		Microsoft::WRL::ComPtr<ID3D11Device> device;
		Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> defaultRenderTargetView;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> depthStencil;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> defaultDepthStencilView;

		SystemEnvironment systemEnvironment;

		App(Microsoft::WRL::ComPtr<ID3D11Device> device):
			device(device) 
			{}
	public:
		void setSwapChain(Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain) {
			this->swapChain = swapChain;
			swapChain->GetDesc1(&swapChainDesc);
		}
		virtual ~App(){}

		SystemEnvironment& getSystemEnvironment(){return systemEnvironment;}
		Microsoft::WRL::ComPtr<ID3DBlob> loadShaderCode(const std::string& shaderFilename);

		virtual HRESULT createResources()
			{return S_OK;}
		virtual HRESULT createSwapChainResources();
		virtual HRESULT releaseResources() { return S_OK; }
		virtual HRESULT releaseSwapChainResources();
		virtual void animate(double dt, double t){}
		virtual bool processMessage( HWND hWnd, 
			UINT uMsg, WPARAM wParam, LPARAM lParam)
			{return false;}
		virtual void render(
			Microsoft::WRL::ComPtr<ID3D11DeviceContext> context){}
	GG_ENDCLASS
} // namespace Egg11
