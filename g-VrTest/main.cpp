#include "Egg/Common.h"
#include <Egg/App.h>
#include "VrTestApp.h"

// Minimal VR-only main: unlike the desktop samples (g-Retam, k-StereoHatching),
// there is no DXGI swap chain here at all -- OpenXR owns its own per-eye
// swapchains (see OpenXRApp.cpp), and device/adapter creation must be driven
// by OpenXR's required adapter LUID (VrTestApp::CreateDeviceAndXrInstance())
// rather than picked before the XR instance exists.

std::unique_ptr<Egg::App> app{ nullptr };

LRESULT CALLBACK WindowProcess(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
	case WM_DESTROY:
		if (app) app->Destroy();
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProcW(windowHandle, message, wParam, lParam);
}

HWND InitWindow(HINSTANCE hInstance) {
	const wchar_t* windowClassName = L"g-VrTestWindowClass";

	WNDCLASSW windowClass;
	ZeroMemory(&windowClass, sizeof(WNDCLASSW));
	windowClass.lpfnWndProc = WindowProcess;
	windowClass.lpszClassName = windowClassName;
	windowClass.hInstance = hInstance;
	RegisterClassW(&windowClass);

	HWND wnd = CreateWindowExW(0, windowClassName, L"g-VrTest",
		WS_OVERLAPPEDWINDOW, 0, 0, 480, 270, NULL, NULL, hInstance, NULL);

	ASSERT(wnd != NULL, "Failed to create window");
	return wnd;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR command, _In_ INT nShowCmd) {

	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_WNDW);

	HWND windowHandle = InitWindow(hInstance);

	com_ptr<ID3D12Debug> debugController{ nullptr };
	DX_API("Failed to create debug layer")
		D3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf()));
	debugController->EnableDebugLayer();

	// Needed to load WIC files (Windows Imaging Component); harmless if unused here.
	DX_API("Failed to initialize COM library")
		CoInitialize(NULL);

	com_ptr<IDXGIFactory4> dxgiFactory{ nullptr };
	DX_API("Failed to create DXGI factory")
		CreateDXGIFactory1(IID_PPV_ARGS(dxgiFactory.GetAddressOf()));

	auto vrApp = std::make_unique<VrTestApp>();

	// Device/queue are created here (on whichever adapter OpenXR requires),
	// not before -- see CreateDeviceAndXrInstance()'s comment in OpenXRApp.h.
	vrApp->CreateDeviceAndXrInstance(dxgiFactory);

	app = std::move(vrApp);
	app->CreateResources();
	app->CreateSwapChainResources();
	app->LoadAssets();

	ShowWindow(windowHandle, nShowCmd);
	MSG winMessage = { 0 };

	while (winMessage.message != WM_QUIT) {
		if (PeekMessage(&winMessage, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&winMessage);
			DispatchMessage(&winMessage);
		}
		else {
			app->Run();
		}
	}

	return 0;
}
