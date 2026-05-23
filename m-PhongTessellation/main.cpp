#include <Egg/App.h>
#include <Egg/Utility.h>
#include "m-PhongTessellationApp.h"

std::unique_ptr<Egg::App> app{ nullptr };

UINT_PTR timerHandle = 0;

void TimerProcess(HWND windowHandle, UINT, UINT_PTR, DWORD) {
    PostMessage(windowHandle, WM_PAINT, 0, 0);
}

LRESULT CALLBACK WindowProcess(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_DESTROY:
        app->Destroy();
        PostQuitMessage(0);
        return 0;
    case WM_SIZE:
        if (app != nullptr) {
            app->Resize(LOWORD(lParam), HIWORD(lParam));
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
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_MOUSEMOVE:
    case WM_MOUSEWHEEL:
    case WM_KILLFOCUS:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
        app->ProcessMessage(windowHandle, message, wParam, lParam);
        break;
    }
    return DefWindowProcW(windowHandle, message, wParam, lParam);
}

HWND InitWindow(HINSTANCE hInstance) {
    const wchar_t* windowClassName = L"PhongTessClass";

    WNDCLASSW windowClass = {};
    windowClass.lpfnWndProc   = WindowProcess;
    windowClass.lpszClassName = windowClassName;
    windowClass.hInstance     = hInstance;
    RegisterClassW(&windowClass);

    HWND wnd = CreateWindowExW(0, windowClassName,
        L"Phong Tessellation (Mesh Shader + Env Map)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        nullptr, nullptr, hInstance, nullptr);

    ASSERT(wnd != nullptr, "Failed to create window");
    return wnd;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ INT nShowCmd) {
    HWND windowHandle = InitWindow(hInstance);

    com_ptr<ID3D12Debug>       debugController;
    com_ptr<IDXGIFactory5>     dxgiFactory;
    com_ptr<IDXGISwapChain3>   swapChain;
    com_ptr<ID3D12Device>      device;
    com_ptr<ID3D12CommandQueue> commandQueue;

    DX_API("Failed to create debug layer")
        D3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf()));
    debugController->EnableDebugLayer();

    DX_API("Failed to initialize COM")
        CoInitialize(nullptr);

    DX_API("Failed to create DXGI factory")
        CreateDXGIFactory1(IID_PPV_ARGS(dxgiFactory.GetAddressOf()));

    std::vector<com_ptr<IDXGIAdapter1>> adapters;
    Egg::Utility::GetAdapters(dxgiFactory.Get(), adapters);

    com_ptr<IDXGIAdapter1> preferredAdapter;
    com_ptr<IDXGIFactory6> dxgiFactory6;
    if (SUCCEEDED(dxgiFactory.As(&dxgiFactory6))) {
        dxgiFactory6->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
            IID_PPV_ARGS(preferredAdapter.GetAddressOf()));
    }
    IUnknown* selectedAdapter = preferredAdapter
        ? preferredAdapter.Get()
        : (adapters.size() > 0 ? adapters[0].Get() : nullptr);

    DX_API("Failed to create D3D12 device")
        D3D12CreateDevice(selectedAdapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(device.GetAddressOf()));

    D3D12_COMMAND_QUEUE_DESC cqDesc = {};
    cqDesc.Type     = D3D12_COMMAND_LIST_TYPE_DIRECT;
    cqDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    cqDesc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
    cqDesc.NodeMask = 0;
    DX_API("Failed to create command queue")
        device->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(commandQueue.GetAddressOf()));

    DXGI_SWAP_CHAIN_DESC1 scDesc = {};
    scDesc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    scDesc.SampleDesc.Count = 1;
    scDesc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.BufferCount      = 2;
    scDesc.Scaling          = DXGI_SCALING_NONE;
    scDesc.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    scDesc.AlphaMode        = DXGI_ALPHA_MODE_IGNORE;

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC scFsDesc = {};
    scFsDesc.RefreshRate = { 60, 1 };
    scFsDesc.Windowed    = true;

    com_ptr<IDXGISwapChain1> tempSwapChain;
    DX_API("Failed to create swap chain")
        dxgiFactory->CreateSwapChainForHwnd(commandQueue.Get(), windowHandle,
            &scDesc, &scFsDesc, nullptr, tempSwapChain.GetAddressOf());
    DX_API("Failed to cast swap chain")
        tempSwapChain.As(&swapChain);
    DX_API("Failed to disable Alt+Enter")
        dxgiFactory->MakeWindowAssociation(windowHandle, DXGI_MWA_NO_ALT_ENTER);

    app = std::make_unique<mPhongTessellationApp>();
    app->SetDevice(device);
    app->SetCommandQueue(commandQueue);
    app->SetSwapChain(swapChain);

    app->CreateResources();
    app->CreateSwapChainResources();
    app->LoadAssets();

    ShowWindow(windowHandle, nShowCmd);

    MSG winMessage = {};
    while (winMessage.message != WM_QUIT) {
        if (PeekMessage(&winMessage, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&winMessage);
            DispatchMessage(&winMessage);
        } else {
            app->Run();
        }
    }

    CoUninitialize();
    return 0;
}
