#pragma once
#include "Egg/Common.h"
#include <GFSDK_Aftermath.h>
#include <GFSDK_Aftermath_GpuCrashDump.h>
#include <GFSDK_Aftermath_GpuCrashDumpDecoding.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <mutex>

// Nsight Aftermath integration -- see the vendored SDK's own Readme.md
// (originally at .../NsightAftermathSDK/2025.5.0.251114/Readme.md) for the
// full API walkthrough this file's callback wiring follows.
//
// Why this exists: the GridRes-change hang (DistanceApp.h's RunReinit,
// blocked on uploadFence.cpuWait()) is a genuine GPU device hang that only
// resolves once the driver's TDR eventually recovers the device (up to
// TdrDelay=600s on this machine) -- a live Nsight Graphics frame capture
// can't complete for a hang like this (confirmed by testing). Aftermath's
// crash-dump path is driven by driver hooks rather than frame completion,
// so it's the one diagnostic that can survive this specific failure mode.
// AftermathMark (DistanceApp.h, one call before every Dispatch() in
// RunTopologyBuild/RunOneRound/RunExtractSurface) is what makes the
// resulting .nv-gpudmp actually name a shader instead of just "something
// hung" -- Nsight Graphics's crash dump inspector reads the marker text
// back out of the dump directly.
class GpuCrashTracker {
public:
    // Must be called before any D3D12 device is created -- see
    // GFSDK_Aftermath_EnableGpuCrashDumps's own doc comment.
    static void EnableGpuCrashDumps() {
        GFSDK_Aftermath_Result r = GFSDK_Aftermath_EnableGpuCrashDumps(
            GFSDK_Aftermath_Version_API,
            GFSDK_Aftermath_GpuCrashDumpWatchedApiFlags_DX,
            GFSDK_Aftermath_GpuCrashDumpFeatureFlags_Default,
            OnCrashDumpCallback,
            nullptr, // no shader debug info -- would need dxc -Zi wired through the whole shader build just to resolve IL/source lines; the event markers below already name the stuck dispatch without it
            OnDescriptionCallback,
            nullptr, // no marker resolve callback needed -- every AftermathMark call below passes its real string + size, never a zero-size token
            nullptr);
        ASSERT(GFSDK_Aftermath_SUCCEED(r), "GFSDK_Aftermath_EnableGpuCrashDumps failed");
    }

    // Call once, right after device creation.
    static void InitializeDevice(ID3D12Device* device) {
        const uint32_t flags =
            GFSDK_Aftermath_FeatureFlags_EnableMarkers |
            GFSDK_Aftermath_FeatureFlags_CallStackCapturing |
            GFSDK_Aftermath_FeatureFlags_EnableResourceTracking |
            GFSDK_Aftermath_FeatureFlags_EnableShaderErrorReporting;
        GFSDK_Aftermath_Result r = GFSDK_Aftermath_DX12_Initialize(GFSDK_Aftermath_Version_API, flags, device);
        ASSERT(GFSDK_Aftermath_SUCCEED(r), "GFSDK_Aftermath_DX12_Initialize failed");
    }

    // The command list must be in the recording state for this call; the
    // returned handle stays valid across that same list's later Close/Reset
    // cycles (see the SDK Readme's "Inserting Event Markers" section).
    static GFSDK_Aftermath_ContextHandle CreateContextHandle(ID3D12GraphicsCommandList* cmd) {
        GFSDK_Aftermath_ContextHandle ctx = nullptr;
        GFSDK_Aftermath_Result r = GFSDK_Aftermath_DX12_CreateContextHandle(cmd, &ctx);
        ASSERT(GFSDK_Aftermath_SUCCEED(r), "GFSDK_Aftermath_DX12_CreateContextHandle failed");
        return ctx;
    }

    static void Mark(GFSDK_Aftermath_ContextHandle ctx, const char* text) {
        if (ctx == nullptr) return;
        GFSDK_Aftermath_SetEventMarker(ctx, (const void*)text, (uint32_t)strlen(text) + 1u);
    }

    // Call after observing DXGI_ERROR_DEVICE_REMOVED/RESET (or any other
    // sign the device just died) -- gives Aftermath's crash-dump thread a
    // bounded window to finish writing the .nv-gpudmp file before the
    // caller does anything that might tear the device down further.
    static void WaitForCrashDump(uint32_t timeoutMs = 5000) {
        GFSDK_Aftermath_CrashDump_Status status = GFSDK_Aftermath_CrashDump_Status_Unknown;
        GFSDK_Aftermath_GetCrashDumpStatus(&status);
        auto start = std::chrono::steady_clock::now();
        while (status != GFSDK_Aftermath_CrashDump_Status_CollectingDataFailed &&
               status != GFSDK_Aftermath_CrashDump_Status_Finished) {
            auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsedMs > timeoutMs) break;
            Sleep(50);
            GFSDK_Aftermath_GetCrashDumpStatus(&status);
        }
    }

private:
    static void OnCrashDump(const void* data, uint32_t size) {
        static std::mutex mutex;
        std::lock_guard<std::mutex> lock(mutex);

        CreateDirectoryA("AftermathDumps", nullptr);
        time_t t = time(nullptr);
        tm lt{};
        localtime_s(&lt, &t);
        char path[256];
        sprintf_s(path, "AftermathDumps/gDistance_%04d%02d%02d_%02d%02d%02d.nv-gpudmp",
            lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec);

        FILE* f = nullptr;
        if (fopen_s(&f, path, "wb") == 0 && f != nullptr) {
            fwrite(data, 1, size, f);
            fclose(f);
        }
    }

    static void OnCrashDumpCallback(const void* pGpuCrashDump, const uint32_t gpuCrashDumpSize, void* /*pUserData*/) {
        OnCrashDump(pGpuCrashDump, gpuCrashDumpSize);
    }

    static void OnDescriptionCallback(PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription addDescription, void* /*pUserData*/) {
        addDescription(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationName, "g-Distance");
    }
};
