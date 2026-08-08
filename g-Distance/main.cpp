#include "Egg/Common.h"
#include <Egg/App.h>
#include <Egg/Utility.h>
#include <chrono>
#include "DistanceApp.h"
#include <imgui.h>
#include <imgui_impl_win32.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

std::unique_ptr<DistanceApp> app{ nullptr };

UINT_PTR timerHandle = 0;

void TimerProcess(HWND windowHandle, UINT a1, UINT_PTR a2, DWORD a3) {
	PostMessage(windowHandle, WM_PAINT, 0, 0);
}

LRESULT CALLBACK WindowProcess(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam) {
	if (ImGui::GetCurrentContext() != nullptr) {
		if (ImGui_ImplWin32_WndProcHandler(windowHandle, message, wParam, lParam))
			return true;
	}

	switch(message) {

	case WM_DESTROY:
		app->ShutdownImGui();
		app->Destroy();
		PostQuitMessage(0);
		return 0;
	case WM_SIZE:
		if(app != nullptr) {
			int height = HIWORD(lParam);
			int width = LOWORD(lParam);
			app->Resize(width, height);
		}
		break;
	case WM_NCLBUTTONDOWN:
		timerHandle = SetTimer(windowHandle, 0, 16, TimerProcess);
		break;
	case WM_NCLBUTTONUP:
		KillTimer(windowHandle, timerHandle);
		break;
	case WM_PAINT:
		app->Run();
		break;
	}

	if (app && ImGui::GetCurrentContext() != nullptr) {
		ImGuiIO& io = ImGui::GetIO();

		bool isKeyboard = (message >= WM_KEYFIRST && message <= WM_KEYLAST);
		bool isMouse     = (message >= WM_MOUSEFIRST && message <= WM_MOUSELAST);
		bool isKeyUp     = (message == WM_KEYUP || message == WM_SYSKEYUP);
		bool isMouseUp   = (message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP);

		bool blocked = false;
		if (isKeyboard && !isKeyUp && io.WantCaptureKeyboard) blocked = true;
		if (isMouse && !isMouseUp && io.WantCaptureMouse) blocked = true;

		if (!blocked &&
			(message == WM_KEYDOWN || message == WM_KEYUP || message == WM_MOUSEMOVE ||
			 message == WM_MOUSEWHEEL || message == WM_KILLFOCUS ||
			 message == WM_LBUTTONDOWN || message == WM_LBUTTONUP ||
			 message == WM_MBUTTONDOWN || message == WM_MBUTTONUP ||
			 message == WM_RBUTTONDOWN || message == WM_RBUTTONUP)) {
			app->ProcessMessage(windowHandle, message, wParam, lParam);
		}
	}

	return DefWindowProcW(windowHandle, message, wParam, lParam);
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
							   L"g-Distance",
							   WS_OVERLAPPEDWINDOW,
							   0,
							   0,
							   1280,
							   720,
							   NULL,
							   NULL,
							   hInstance,
							   NULL);

	ASSERT(wnd != NULL, "Failed to create window");

	return wnd;
}


int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR command, _In_ INT nShowCmd) {

	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_WNDW);

	HWND windowHandle = InitWindow(hInstance);
	com_ptr<ID3D12Debug> debugController{ nullptr };
	com_ptr<IDXGIFactory6> dxgiFactory{ nullptr };
	com_ptr<IDXGISwapChain3> swapChain{ nullptr };
	com_ptr<ID3D12Device> device{ nullptr };
	com_ptr<ID3D12CommandQueue> commandQueue{ nullptr };

	DX_API("Failed to create debug layer")
		D3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf()));

	DX_API("Failed to initialize COM library (ImportTexture)")
		CoInitialize(NULL);

	DX_API("Failed to create DXGI factory")
		CreateDXGIFactory1(IID_PPV_ARGS(dxgiFactory.GetAddressOf()));

	std::vector<com_ptr<IDXGIAdapter1>> adapters;
	Egg::Utility::GetAdapters(dxgiFactory.Get(), adapters);

	IUnknown* selectedAdapter = NULL;

	for (auto adapter : adapters) {
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);
		if (desc.VendorId == 0x10de) {
			selectedAdapter = adapter.Get();
		}
	}
	if (selectedAdapter == NULL) {
		MessageBox(NULL, "null adapter", "0", 0);
	}

	DX_API("Failed to create D3D Device")
		D3D12CreateDevice(selectedAdapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(device.GetAddressOf()));

	D3D12_FEATURE_DATA_D3D12_OPTIONS1 fedup;
	device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1, &fedup, sizeof(fedup));
	if (fedup.WaveLaneCountMax != 0x20) {
		MessageBox(NULL, "WaveLaneCountMax != 0x20", std::to_string(fedup.WaveLaneCountMax).c_str(), 0);
		return -1;
	}

	D3D12_COMMAND_QUEUE_DESC commandQueueDesc;
	commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	commandQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	commandQueueDesc.NodeMask = 0;

	DX_API("Failed to create command queue")
		device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(commandQueue.GetAddressOf()));

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = { 0 };
	swapChainDesc.Width = 0;
	swapChainDesc.Height = 0;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Stereo = false;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.Scaling = DXGI_SCALING_NONE;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
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

	DX_API("Failed to make window association")
		dxgiFactory->MakeWindowAssociation(windowHandle, DXGI_MWA_NO_ALT_ENTER);

	app = std::make_unique<DistanceApp>();
	app->SetDevice(device);
	app->SetCommandQueue(commandQueue);
	app->SetSwapChain(swapChain);

	app->CreateResources();
	app->CreateSwapChainResources();
	app->LoadAssets();
	app->InitImGui(windowHandle);

	ShowWindow(windowHandle, nShowCmd);
	MSG winMessage = { 0 };

	auto startTime = std::chrono::system_clock::now();
	auto time = startTime;

	while(winMessage.message != WM_QUIT) {
		if(PeekMessage(&winMessage, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&winMessage);
			DispatchMessage(&winMessage);
		} else {
			app->Run();
		}
	}

	CoUninitialize();

	return 0;
}
