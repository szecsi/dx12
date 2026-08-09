#include "OpenXRApp.h"
#include <cmath>

namespace Egg {

// ---- Helper: error check -----------------------------------------------

static void XrCheck(XrResult result, const char* msg) {
    ASSERT(result >= 0, msg);
}

// ---- Descriptor handle accessors ----------------------------------------

D3D12_CPU_DESCRIPTOR_HANDLE OpenXRApp::GetEyeRtv(uint32_t eye, uint32_t imageIndex) const {
    CD3DX12_CPU_DESCRIPTOR_HANDLE h(eyeRtvHeap->GetCPUDescriptorHandleForHeapStart());
    h.Offset((int)(eyeRtvOffset[eye] + imageIndex), eyeRtvIncrSize);
    return h;
}

D3D12_CPU_DESCRIPTOR_HANDLE OpenXRApp::GetEyeDsv(uint32_t eye) const {
    CD3DX12_CPU_DESCRIPTOR_HANDLE h(eyeDsvHeap->GetCPUDescriptorHandleForHeapStart());
    h.Offset((int)eye, eyeDsvIncrSize);
    return h;
}

// ---- GPU sync (no swapchain) --------------------------------------------

void OpenXRApp::WaitForGpu() {
    DX_API("Failed to signal fence")
        commandQueue->Signal(fence.Get(), fenceValue);
    DX_API("Failed to register fence completion")
        fence->SetEventOnCompletion(fenceValue, fenceEvent);
    WaitForSingleObject(fenceEvent, INFINITE);
    ++fenceValue;
}

// ---- OpenXR init ---------------------------------------------------------

void OpenXRApp::InitXrInstance() {
    XrApplicationInfo appInfo = {};
    strcpy_s(appInfo.applicationName, "Egg VR App");
    appInfo.applicationVersion = 1;
    strcpy_s(appInfo.engineName, "Egg");
    appInfo.engineVersion = 1;
    appInfo.apiVersion = XR_CURRENT_API_VERSION;

    const char* extensions[] = { XR_KHR_D3D12_ENABLE_EXTENSION_NAME };

    XrInstanceCreateInfo createInfo = { XR_TYPE_INSTANCE_CREATE_INFO };
    createInfo.applicationInfo = appInfo;
    createInfo.enabledExtensionCount = 1;
    createInfo.enabledExtensionNames = extensions;

    XrCheck(xrCreateInstance(&createInfo, &xrInstance),
        "Failed to create XrInstance — is the OpenXR runtime running?");

    XrSystemGetInfo sysInfo = { XR_TYPE_SYSTEM_GET_INFO };
    sysInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrCheck(xrGetSystem(xrInstance, &sysInfo, &xrSystemId),
        "Failed to find an HMD — is the headset connected?");
}

void OpenXRApp::InitXrSession() {
    // Must call xrGetD3D12GraphicsRequirementsKHR before creating the session
    PFN_xrGetD3D12GraphicsRequirementsKHR pfnGetReqs = nullptr;
    XrCheck(xrGetInstanceProcAddr(xrInstance, "xrGetD3D12GraphicsRequirementsKHR",
        (PFN_xrVoidFunction*)&pfnGetReqs),
        "Failed to load xrGetD3D12GraphicsRequirementsKHR");

    XrGraphicsRequirementsD3D12KHR reqs = { XR_TYPE_GRAPHICS_REQUIREMENTS_D3D12_KHR };
    XrCheck(pfnGetReqs(xrInstance, xrSystemId, &reqs),
        "Failed to get D3D12 graphics requirements");

    XrGraphicsBindingD3D12KHR binding = { XR_TYPE_GRAPHICS_BINDING_D3D12_KHR };
    binding.device = device.Get();
    binding.queue = commandQueue.Get();

    XrSessionCreateInfo sessionInfo = { XR_TYPE_SESSION_CREATE_INFO };
    sessionInfo.next = &binding;
    sessionInfo.systemId = xrSystemId;
    XrCheck(xrCreateSession(xrInstance, &sessionInfo, &xrSession),
        "Failed to create XrSession");

    // Local reference space: origin at the user's initial head position
    XrReferenceSpaceCreateInfo spaceInfo = { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
    spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    spaceInfo.poseInReferenceSpace.orientation = { 0.0f, 0.0f, 0.0f, 1.0f };
    spaceInfo.poseInReferenceSpace.position = { 0.0f, 0.0f, 0.0f };
    XrCheck(xrCreateReferenceSpace(xrSession, &spaceInfo, &xrLocalSpace),
        "Failed to create reference space");

    // Query recommended per-eye render dimensions
    uint32_t viewCount = 0;
    xrEnumerateViewConfigurationViews(xrInstance, xrSystemId,
        XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &viewCount, nullptr);
    ASSERT(viewCount == EYE_COUNT, "Expected exactly 2 views for stereo HMD");

    for (auto& v : viewConfigViews) v = { XR_TYPE_VIEW_CONFIGURATION_VIEW };
    xrEnumerateViewConfigurationViews(xrInstance, xrSystemId,
        XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
        EYE_COUNT, &viewCount, viewConfigViews);
}

void OpenXRApp::CreateXrSwapchains() {
    for (uint32_t eye = 0; eye < EYE_COUNT; eye++) {
        uint32_t w = viewConfigViews[eye].recommendedImageRectWidth;
        uint32_t h = viewConfigViews[eye].recommendedImageRectHeight;
        uint32_t s = viewConfigViews[eye].recommendedSwapchainSampleCount;

        XrSwapchainCreateInfo info = { XR_TYPE_SWAPCHAIN_CREATE_INFO };
        info.arraySize = 1;
        info.mipCount = 1;
        info.faceCount = 1;
        info.format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        info.width = w;
        info.height = h;
        info.sampleCount = s;
        info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;

        XrCheck(xrCreateSwapchain(xrSession, &info, &xrSwapchains[eye]),
            "Failed to create XR swapchain");

        uint32_t imgCount = 0;
        xrEnumerateSwapchainImages(xrSwapchains[eye], 0, &imgCount, nullptr);
        xrSwapchainImages[eye].resize(imgCount, { XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR });
        xrEnumerateSwapchainImages(xrSwapchains[eye], imgCount, &imgCount,
            reinterpret_cast<XrSwapchainImageBaseHeader*>(xrSwapchainImages[eye].data()));

        eyeViewports[eye] = { 0.0f, 0.0f, (float)w, (float)h, 0.0f, 1.0f };
        eyeScissorRects[eye] = { 0, 0, (LONG)w, (LONG)h };
    }
}

// ---- OpenXR event polling ------------------------------------------------

void OpenXRApp::PollXrEvents() {
    XrEventDataBuffer ev = { XR_TYPE_EVENT_DATA_BUFFER };
    while (xrPollEvent(xrInstance, &ev) == XR_SUCCESS) {
        switch (ev.type) {
        case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
            auto& stateEv = reinterpret_cast<XrEventDataSessionStateChanged&>(ev);
            xrSessionState = stateEv.state;

            if (stateEv.state == XR_SESSION_STATE_READY) {
                XrSessionBeginInfo beginInfo = { XR_TYPE_SESSION_BEGIN_INFO };
                beginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                XrCheck(xrBeginSession(xrSession, &beginInfo), "Failed to begin XR session");
                xrSessionRunning = true;
            } else if (stateEv.state == XR_SESSION_STATE_STOPPING) {
                xrEndSession(xrSession);
                xrSessionRunning = false;
            } else if (stateEv.state == XR_SESSION_STATE_EXITING ||
                       stateEv.state == XR_SESSION_STATE_LOSS_PENDING) {
                xrSessionRunning = false;
                Stop();
            }
            break;
        }
        default:
            break;
        }
        ev = { XR_TYPE_EVENT_DATA_BUFFER };
    }
}

// ---- Matrix math ---------------------------------------------------------

Egg::Math::float4x4 OpenXRApp::XrPoseToViewMatrix(const XrPosef& pose) {
    // Build camera-to-world rotation matrix from unit quaternion (column-vector convention)
    float qx = pose.orientation.x, qy = pose.orientation.y,
          qz = pose.orientation.z, qw = pose.orientation.w;

    float R00 = 1.0f - 2.0f*(qy*qy + qz*qz);
    float R01 = 2.0f*(qx*qy - qw*qz);
    float R02 = 2.0f*(qx*qz + qw*qy);
    float R10 = 2.0f*(qx*qy + qw*qz);
    float R11 = 1.0f - 2.0f*(qx*qx + qz*qz);
    float R12 = 2.0f*(qy*qz - qw*qx);
    float R20 = 2.0f*(qx*qz - qw*qy);
    float R21 = 2.0f*(qy*qz + qw*qx);
    float R22 = 1.0f - 2.0f*(qx*qx + qy*qy);

    float px = pose.position.x, py = pose.position.y, pz = pose.position.z;

    // World-to-camera: V = R^T * T(-p)
    // Translation column = -R^T * p  (note: R^T_{row i} = R_{col i})
    float tx = -(R00*px + R10*py + R20*pz);
    float ty = -(R01*px + R11*py + R21*pz);
    float tz = -(R02*px + R12*py + R22*pz);

    return Egg::Math::float4x4(
        R00, R10, R20, tx,
        R01, R11, R21, ty,
        R02, R12, R22, tz,
        0,   0,   0,   1
    );
}

Egg::Math::float4x4 OpenXRApp::XrFovToProjectionMatrix(const XrFovf& fov, float nearZ, float farZ) {
    // Asymmetric off-centre perspective (right-handed, DX NDC depth [0,1]).
    // w_clip = -v_z   (perspective divide on negated Z for right-handed space)
    float tanL = tanf(fov.angleLeft);   // negative
    float tanR = tanf(fov.angleRight);  // positive
    float tanU = tanf(fov.angleUp);     // positive
    float tanD = tanf(fov.angleDown);   // negative

    float rL = tanR - tanL;
    float rU = tanU - tanD;

    float a = 2.0f / rL;
    float b = 2.0f / rU;
    float c = (tanR + tanL) / rL;   // horizontal offset
    float d = (tanU + tanD) / rU;   // vertical offset
    float e = -farZ / (farZ - nearZ);
    float f = -(nearZ * farZ) / (farZ - nearZ);

    return Egg::Math::float4x4(
        a, 0,  c,  0,
        0, b,  d,  0,
        0, 0,  e,  f,
        0, 0, -1,  0
    );
}

void OpenXRApp::BuildEyeMatrices() {
    for (uint32_t eye = 0; eye < EYE_COUNT; eye++) {
        eyeViewMatrix[eye] = XrPoseToViewMatrix(xrViews[eye].pose);
        eyeProjMatrix[eye] = XrFovToProjectionMatrix(xrViews[eye].fov, 0.01f, 1000.0f);
    }
}

// ---- Lifecycle -----------------------------------------------------------

void OpenXRApp::CreateResources() {
    // Replicate SimpleApp::CreateResources() but skip WaitForPreviousFrame(),
    // which queries swapChain->GetCurrentBackBufferIndex() — we have no DXGI swapchain.
    App::CreateResources();

    psoManager = PsoManager::Create(device);

    DX_API("Failed to create command allocator")
        device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(commandAllocator.GetAddressOf()));

    DX_API("Failed to create command list")
        device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
            commandAllocator.Get(), nullptr, IID_PPV_ARGS(commandList.GetAddressOf()));
    commandList->Close();

    WaitForGpu();

    InitXrInstance();
    InitXrSession();
    CreateXrSwapchains();
}

void OpenXRApp::CreateSwapChainResources() {
    // Do NOT call base class — base tries to query the DXGI swapchain.

    eyeRtvIncrSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    eyeDsvIncrSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

    // Total RTV count = sum of all swapchain image counts across eyes
    uint32_t totalImages = 0;
    for (uint32_t eye = 0; eye < EYE_COUNT; eye++) {
        eyeRtvOffset[eye] = totalImages;
        totalImages += (uint32_t)xrSwapchainImages[eye].size();
    }

    D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
    rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvDesc.NumDescriptors = totalImages;
    rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    DX_API("Failed to create eye RTV descriptor heap")
        device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(eyeRtvHeap.GetAddressOf()));

    D3D12_DESCRIPTOR_HEAP_DESC dsvDesc = {};
    dsvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvDesc.NumDescriptors = EYE_COUNT;
    dsvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    DX_API("Failed to create eye DSV descriptor heap")
        device->CreateDescriptorHeap(&dsvDesc, IID_PPV_ARGS(eyeDsvHeap.GetAddressOf()));

    // RTVs: one per XR swapchain image
    for (uint32_t eye = 0; eye < EYE_COUNT; eye++) {
        for (uint32_t img = 0; img < (uint32_t)xrSwapchainImages[eye].size(); img++) {
            auto handle = GetEyeRtv(eye, img);
            device->CreateRenderTargetView(xrSwapchainImages[eye][img].texture, nullptr, handle);
        }
    }

    // DSVs + depth buffers: one per eye
    D3D12_CLEAR_VALUE clearVal = {};
    clearVal.Format = DXGI_FORMAT_D32_FLOAT;
    clearVal.DepthStencil.Depth = 1.0f;

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvViewDesc = {};
    dsvViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

    for (uint32_t eye = 0; eye < EYE_COUNT; eye++) {
        uint32_t w = viewConfigViews[eye].recommendedImageRectWidth;
        uint32_t h = viewConfigViews[eye].recommendedImageRectHeight;

        DX_API("Failed to create eye depth buffer")
            device->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                D3D12_HEAP_FLAG_NONE,
                &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, w, h,
                    1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
                D3D12_RESOURCE_STATE_DEPTH_WRITE,
                &clearVal,
                IID_PPV_ARGS(eyeDepthBuffers[eye].GetAddressOf()));

        device->CreateDepthStencilView(eyeDepthBuffers[eye].Get(), &dsvViewDesc, GetEyeDsv(eye));
    }
}

void OpenXRApp::Render() {
    PollXrEvents();
    if (!xrSessionRunning) return;

    // --- OpenXR frame loop ---
    XrFrameWaitInfo waitInfo = { XR_TYPE_FRAME_WAIT_INFO };
    xrFrameState = { XR_TYPE_FRAME_STATE };
    xrWaitFrame(xrSession, &waitInfo, &xrFrameState);
    xrBeginFrame(xrSession, nullptr);

    XrCompositionLayerProjectionView projViews[EYE_COUNT] = {};
    bool didRender = (xrFrameState.shouldRender == XR_TRUE);

    if (didRender) {
        // Locate eye poses at the predicted display time
        XrViewLocateInfo locateInfo = { XR_TYPE_VIEW_LOCATE_INFO };
        locateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        locateInfo.displayTime = xrFrameState.predictedDisplayTime;
        locateInfo.space = xrLocalSpace;

        XrViewState viewState = { XR_TYPE_VIEW_STATE };
        uint32_t viewCount = EYE_COUNT;
        for (auto& v : xrViews) v = { XR_TYPE_VIEW };
        xrLocateViews(xrSession, &locateInfo, &viewState, EYE_COUNT, &viewCount, xrViews);

        BuildEyeMatrices();

        for (uint32_t eye = 0; eye < EYE_COUNT; eye++) {
            // Acquire a compositor-owned swapchain image
            XrSwapchainImageAcquireInfo acquireInfo = { XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
            xrAcquireSwapchainImage(xrSwapchains[eye], &acquireInfo, &xrCurrentImageIndex[eye]);

            XrSwapchainImageWaitInfo imgWait = { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
            imgWait.timeout = XR_INFINITE_DURATION;
            xrWaitSwapchainImage(xrSwapchains[eye], &imgWait);

            // Record rendering commands
            xrCurrentEye = eye;
            DX_API("Failed to reset command allocator")
                commandAllocator->Reset();
            DX_API("Failed to reset command list")
                commandList->Reset(commandAllocator.Get(), nullptr);

            // Transition color RT from compositor ownership to render target
            D3D12_RESOURCE_BARRIER barrierIn = CD3DX12_RESOURCE_BARRIER::Transition(
                xrSwapchainImages[eye][xrCurrentImageIndex[eye]].texture,
                D3D12_RESOURCE_STATE_COMMON,
                D3D12_RESOURCE_STATE_RENDER_TARGET);
            commandList->ResourceBarrier(1, &barrierIn);

            PopulateEyeCommandList();

            // Return color RT to compositor
            D3D12_RESOURCE_BARRIER barrierOut = CD3DX12_RESOURCE_BARRIER::Transition(
                xrSwapchainImages[eye][xrCurrentImageIndex[eye]].texture,
                D3D12_RESOURCE_STATE_RENDER_TARGET,
                D3D12_RESOURCE_STATE_COMMON);
            commandList->ResourceBarrier(1, &barrierOut);

            DX_API("Failed to close command list")
                commandList->Close();

            ID3D12CommandList* lists[] = { commandList.Get() };
            commandQueue->ExecuteCommandLists(1, lists);
            WaitForGpu();

            // Return image to the compositor
            XrSwapchainImageReleaseInfo releaseInfo = { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
            xrReleaseSwapchainImage(xrSwapchains[eye], &releaseInfo);

            projViews[eye] = { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW };
            projViews[eye].pose = xrViews[eye].pose;
            projViews[eye].fov = xrViews[eye].fov;
            projViews[eye].subImage.swapchain = xrSwapchains[eye];
            projViews[eye].subImage.imageRect.offset = { 0, 0 };
            projViews[eye].subImage.imageRect.extent = {
                (int32_t)viewConfigViews[eye].recommendedImageRectWidth,
                (int32_t)viewConfigViews[eye].recommendedImageRectHeight
            };
        }
    }

    // Submit frame to compositor
    XrCompositionLayerProjection projLayer = { XR_TYPE_COMPOSITION_LAYER_PROJECTION };
    projLayer.space = xrLocalSpace;
    projLayer.viewCount = EYE_COUNT;
    projLayer.views = projViews;

    const XrCompositionLayerBaseHeader* layers[] = {
        reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projLayer)
    };

    XrFrameEndInfo endInfo = { XR_TYPE_FRAME_END_INFO };
    endInfo.displayTime = xrFrameState.predictedDisplayTime;
    endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    endInfo.layerCount = didRender ? 1u : 0u;
    endInfo.layers = didRender ? layers : nullptr;
    xrEndFrame(xrSession, &endInfo);
}

void OpenXRApp::ReleaseSwapChainResources() {
    for (uint32_t eye = 0; eye < EYE_COUNT; eye++) {
        eyeDepthBuffers[eye].Reset();
        if (xrSwapchains[eye] != XR_NULL_HANDLE) {
            xrDestroySwapchain(xrSwapchains[eye]);
            xrSwapchains[eye] = XR_NULL_HANDLE;
        }
        xrSwapchainImages[eye].clear();
    }
    eyeRtvHeap.Reset();
    eyeDsvHeap.Reset();
}

void OpenXRApp::ReleaseResources() {
    if (xrLocalSpace != XR_NULL_HANDLE) {
        xrDestroySpace(xrLocalSpace);
        xrLocalSpace = XR_NULL_HANDLE;
    }
    if (xrSession != XR_NULL_HANDLE) {
        xrDestroySession(xrSession);
        xrSession = XR_NULL_HANDLE;
    }
    if (xrInstance != XR_NULL_HANDLE) {
        xrDestroyInstance(xrInstance);
        xrInstance = XR_NULL_HANDLE;
    }

    psoManager = nullptr;
    commandList.Reset();
    fence.Reset();
    commandAllocator.Reset();
    App::ReleaseResources();
}

void OpenXRApp::Destroy() {
    WaitForGpu();
    ReleaseSwapChainResources();
    ReleaseResources();
    ReleaseAssets();
}

}
