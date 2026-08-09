#pragma once
#include "../Common.h"
#include "../SimpleApp.h"
#include "../Math/Float4x4.h"

#define XR_USE_GRAPHICS_API_D3D12
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

namespace Egg {

GG_SUBCLASS(OpenXRApp, Egg::SimpleApp)
protected:
    static constexpr uint32_t EYE_COUNT = 2;

    // --- OpenXR session ---
    XrInstance xrInstance = XR_NULL_HANDLE;
    XrSystemId xrSystemId = XR_NULL_SYSTEM_ID;
    XrSession xrSession = XR_NULL_HANDLE;
    XrSpace xrLocalSpace = XR_NULL_HANDLE;
    XrSessionState xrSessionState = XR_SESSION_STATE_UNKNOWN;
    bool xrSessionRunning = false;

    // --- Per-eye swapchains ---
    XrSwapchain xrSwapchains[EYE_COUNT] = {};
    std::vector<XrSwapchainImageD3D12KHR> xrSwapchainImages[EYE_COUNT];
    XrViewConfigurationView viewConfigViews[EYE_COUNT] = {};

    // --- Per-eye render targets ---
    com_ptr<ID3D12DescriptorHeap> eyeRtvHeap;
    com_ptr<ID3D12DescriptorHeap> eyeDsvHeap;
    com_ptr<ID3D12Resource> eyeDepthBuffers[EYE_COUNT];
    // RTV heap layout: eye0 images first, then eye1 images
    uint32_t eyeRtvOffset[EYE_COUNT] = {};
    uint32_t eyeRtvIncrSize = 0;
    uint32_t eyeDsvIncrSize = 0;

    // --- Per-frame state ---
    XrFrameState xrFrameState = { XR_TYPE_FRAME_STATE };
    XrView xrViews[EYE_COUNT] = {};

    // Active eye index during PopulateEyeCommandList; current swapchain image index per eye
    uint32_t xrCurrentEye = 0;
    uint32_t xrCurrentImageIndex[EYE_COUNT] = {};

    // Eye transforms available to subclasses for uploading to shader constant buffers.
    // Coordinate system: right-handed (OpenXR), view-space Z points backward (away from viewer).
    // Projection maps to DirectX NDC with depth in [0, 1].
    Egg::Math::float4x4 eyeViewMatrix[EYE_COUNT];
    Egg::Math::float4x4 eyeProjMatrix[EYE_COUNT];
    D3D12_VIEWPORT eyeViewports[EYE_COUNT] = {};
    D3D12_RECT eyeScissorRects[EYE_COUNT] = {};

    // Helper accessors for use inside PopulateEyeCommandList
    D3D12_CPU_DESCRIPTOR_HANDLE GetEyeRtv(uint32_t eye, uint32_t imageIndex) const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetEyeDsv(uint32_t eye) const;

    // Override this to record rendering commands for one eye.
    // Called once per eye per frame with an already-open command list.
    // Do NOT reset or close the command list; only record draw/clear commands.
    // Use xrCurrentEye, eyeViewMatrix[xrCurrentEye], eyeProjMatrix[xrCurrentEye],
    // GetEyeRtv(xrCurrentEye, xrCurrentImageIndex[xrCurrentEye]), GetEyeDsv(xrCurrentEye).
    virtual void PopulateEyeCommandList() = 0;

public:
    virtual void CreateResources() override;
    virtual void CreateSwapChainResources() override;
    virtual void Render() override;
    virtual void ReleaseSwapChainResources() override;
    virtual void ReleaseResources() override;
    virtual void Resize(int width, int height) override {}
    virtual void Destroy() override;

protected:
    // SimpleApp's PopulateCommandList is unused in the VR rendering path.
    virtual void PopulateCommandList() override final {}

private:
    void InitXrInstance();
    void InitXrSession();
    void CreateXrSwapchains();
    void PollXrEvents();
    void WaitForGpu();
    void BuildEyeMatrices();
    static Egg::Math::float4x4 XrPoseToViewMatrix(const XrPosef& pose);
    static Egg::Math::float4x4 XrFovToProjectionMatrix(const XrFovf& fov, float nearZ, float farZ);
GG_ENDCLASS

}
