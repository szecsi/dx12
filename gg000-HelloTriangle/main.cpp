#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <vector>

#define ASSERT(trueMeansOk, msg) if(!trueMeansOk) { MessageBoxA(NULL, msg, "Error!", MB_ICONSTOP | MB_OK); PostQuitMessage(1); }

class HResultTester {
public:
	const char * Message;

	HResultTester(const char * msg) : Message{ msg } { }

	void operator<<(HRESULT hr) {
		ASSERT(hr == S_OK, Message);
	}
};

#define DX_API(msg) HResultTester(msg) <<

template<typename T>
using com_ptr = Microsoft::WRL::ComPtr<T>;

LRESULT CALLBACK WindowProcess(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam) {
	switch(message) {
		case WM_DESTROY:
			// adds a WM_QUIT message to the message queue
			PostQuitMessage(0);
			return 0;
	}

	return DefWindowProc(windowHandle, message, wParam, lParam);
}

HWND InitWindow(HINSTANCE hInstance) {
	const wchar_t * windowClassName = L"ClassName";

	WNDCLASSW windowClass;
	ZeroMemory(&windowClass, sizeof(WNDCLASSW));

	windowClass.lpfnWndProc = WindowProcess;
	windowClass.lpszClassName = windowClassName;
	windowClass.hInstance = hInstance;

	RegisterClassW(&windowClass);

	HWND wnd = CreateWindowExW(0,
		windowClassName,
		L"Hello Triangle",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		NULL,
		NULL,
		hInstance,
		NULL);

	ASSERT(wnd != NULL, "Failed to create window");

	return wnd;
}

void GetAdapters(IDXGIFactory6 * dxgiFactory, std::vector<com_ptr<IDXGIAdapter1>> & adapters) {
	HRESULT adapterQueryResult;
	unsigned int adapterId = 0;
	OutputDebugStringW(L"Detected Video Adapters:\n");
	do {
		com_ptr<IDXGIAdapter1> tempAdapter{ nullptr };
		adapterQueryResult = dxgiFactory->EnumAdapters1(adapterId, tempAdapter.GetAddressOf());

		if(adapterQueryResult != S_OK && adapterQueryResult != DXGI_ERROR_NOT_FOUND) {
			ASSERT(false, "Failed to query DXGI adapter");
		}

		if(tempAdapter != nullptr) {
			DXGI_ADAPTER_DESC desc;
			tempAdapter->GetDesc(&desc);

			OutputDebugStringW(L"\t");
			OutputDebugStringW(desc.Description);
			OutputDebugStringW(L"\n");

			adapters.push_back(std::move(tempAdapter));
		}

		adapterId++;
	} while(adapterQueryResult != DXGI_ERROR_NOT_FOUND);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, LPWSTR command, INT nShowCmd) {
	HWND windowHandle = InitWindow(hInstance);
	
	// DirectX stuff
	com_ptr<ID3D12Debug> debugController{ nullptr };
	com_ptr<IDXGIFactory6> dxgiFactory{ nullptr };
	com_ptr<IDXGISwapChain3> swapChain{ nullptr };
	com_ptr<ID3D12Device> device{ nullptr };
	com_ptr<ID3D12CommandQueue> commandQueue{ nullptr };

	// debug controller 
	DX_API("Failed to create debug layer") 
		D3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf()));

	debugController->EnableDebugLayer();

	DX_API("Failed to create DXGI factory")
		CreateDXGIFactory1(IID_PPV_ARGS(dxgiFactory.GetAddressOf()));

	std::vector<com_ptr<IDXGIAdapter1>> adapters;
	GetAdapters(dxgiFactory.Get(), adapters);

	// select your adapter here, NULL = system default
	IUnknown * selectedAdapter = (adapters.size() > 0) ? adapters[0].Get() : NULL;

	DX_API("Failed to create D3D Device")
		D3D12CreateDevice(selectedAdapter, D3D_FEATURE_LEVEL_11_1, IID_PPV_ARGS(device.GetAddressOf()));

	D3D12_COMMAND_QUEUE_DESC commandQueueDesc;
	commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	commandQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	commandQueueDesc.NodeMask = 0;

	DX_API("Failed to create command queue")
		device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(commandQueue.GetAddressOf()));


	// swap chain creation
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = { 0 };
	// if you specify width/height as 0, the CreateSwapChainForHwnd will query it from the output window
	swapChainDesc.Width = 0;
	swapChainDesc.Height = 0;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Stereo = false;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 2; // back buffer depth
	swapChainDesc.Scaling = DXGI_SCALING_NONE;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
	swapChainDesc.Flags = 0;

	DXGI_SWAP_CHAIN_FULLSCREEN_DESC swapChainFullscreenDesc = { 0 };
	swapChainFullscreenDesc.RefreshRate = DXGI_RATIONAL{ 60, 1 };
	swapChainFullscreenDesc.Windowed = true;
	swapChainFullscreenDesc.Scaling = DXGI_MODE_SCALING_CENTERED;
	swapChainFullscreenDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UPPER_FIELD_FIRST;

	com_ptr<IDXGISwapChain1> tempSwapChain;

	DX_API("Failed to create swap chain for HWND")
		dxgiFactory->CreateSwapChainForHwnd(commandQueue.Get(), windowHandle, &swapChainDesc, &swapChainFullscreenDesc, NULL, tempSwapChain.GetAddressOf());

	DX_API("Failed to cast swap chain")
		tempSwapChain.As(&swapChain);

	DX_API("Failed to make window association") // disable ALT+Enter shortcut to full screen mode
		dxgiFactory->MakeWindowAssociation(windowHandle, DXGI_MWA_NO_ALT_ENTER);



	ShowWindow(windowHandle, nShowCmd);
	MSG winMessage = { 0 };

	while(winMessage.message != WM_QUIT) {
		if(PeekMessage(&winMessage, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&winMessage);
			DispatchMessage(&winMessage);
		}
	}

	return 0;
}
