#pragma once

#include <Egg/SimpleApp.h>
#include <Egg/Importer.h>
#include <DirectXTex/DirectXTex.h>

#include <vector>
#include <queue>
#include <algorithm>
#include <cmath>
#include <limits>
#include <cstdint>

class UVMaskApp : public Egg::SimpleApp {
    static const int TEX_SIZE = 1024;

    Egg::Mesh::Shaded::P shadedMesh;
    com_ptr<ID3D12Resource> maskTexture;
    com_ptr<ID3D12Resource> readbackBuffer;
    com_ptr<ID3D12DescriptorHeap> maskRtvHeap;

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT readbackFootprint;
    UINT   readbackNumRows;
    UINT64 readbackRowSize;
    UINT64 readbackTotalSize;

    bool maskRendered = false;
    bool dataSaved    = false;

    void ProcessAndSave() {
        const int W = TEX_SIZE, H = TEX_SIZE;

        void* mappedData = nullptr;
        CD3DX12_RANGE readRange(0, (SIZE_T)readbackTotalSize);
        DX_API("Failed to map readback buffer")
            readbackBuffer->Map(0, &readRange, &mappedData);

        // Build boolean mask from the red channel of the RGBA8 readback
        std::vector<bool> used(W * H, false);
        const uint8_t* src  = static_cast<const uint8_t*>(mappedData);
        UINT rowPitch = readbackFootprint.Footprint.RowPitch;
        for (int y = 0; y < H; ++y) {
            const uint8_t* row = src + (UINT64)y * rowPitch;
            for (int x = 0; x < W; ++x)
                used[y * W + x] = (row[x * 4] > 0);
        }

        readbackBuffer->Unmap(0, nullptr);

        // Distance transform: Dijkstra seeded from every unused pixel (dist=0).
        // Result: dist[i] = distance from pixel i to the nearest unused texel.
        const float INF = std::numeric_limits<float>::max();
        std::vector<float> dist(W * H, INF);

        struct Entry {
            float d;
            int x, y;
            bool operator>(const Entry& o) const { return d > o.d; }
        };
        std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> pq;

        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                if (!used[y * W + x]) {
                    dist[y * W + x] = 0.0f;
                    pq.push({ 0.0f, x, y });
                }
            }
        }

        // 8-connected neighbourhood with Euclidean step sizes
        const int   ndx[] = { -1,  0,  1, -1,  1, -1,  0,  1 };
        const int   ndy[] = { -1, -1, -1,  0,  0,  1,  1,  1 };
        const float nds[] = { 1.41421f, 1.0f, 1.41421f, 1.0f,
                               1.0f, 1.41421f, 1.0f, 1.41421f };

        while (!pq.empty()) {
            Entry e = pq.top(); pq.pop();
            if (e.d > dist[e.y * W + e.x]) continue;
            for (int k = 0; k < 8; ++k) {
                int nx = e.x + ndx[k], ny = e.y + ndy[k];
                if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
                float nd = e.d + nds[k];
                if (nd < dist[ny * W + nx]) {
                    dist[ny * W + nx] = nd;
                    pq.push({ nd, nx, ny });
                }
            }
        }

        // Build R16 output: distance in UV space (pixels / TEX_SIZE), flipped vertically
        std::vector<uint16_t> pixels(W * H);
        for (int y = 0; y < H; ++y) {
            int dstY = H - 1 - y;
            for (int x = 0; x < W; ++x) {
                int srcI = y * W + x;
                float d = (dist[srcI] == INF) ? 0.0f : dist[srcI];
                pixels[dstY * W + x] = used[srcI] ? static_cast<uint16_t>(std::min(d / W * 65535.0f, 65535.0f)) : 0;
            }
        }

        DirectX::Image img;
        img.width      = W;
        img.height     = H;
        img.format     = DXGI_FORMAT_R16_UNORM;
        img.rowPitch   = W * 2;
        img.slicePitch = (size_t)W * H * 2;
        img.pixels     = reinterpret_cast<uint8_t*>(pixels.data());

        DX_API("Failed to save uvmask.png")
            DirectX::SaveToWICFile(img, DirectX::WIC_FLAGS_NONE,
                DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), L"uvmask.png");
    }

public:
    virtual void Update(float dt, float T) override {
        if (maskRendered && !dataSaved) {
            // GPU finished on previous frame (WaitForPreviousFrame already called)
            ProcessAndSave();
            dataSaved = true;
            Stop();
        }
    }

    virtual void PopulateCommandList() override {
        commandAllocator->Reset();
        commandList->Reset(commandAllocator.Get(), nullptr);

        if (!maskRendered) {
            // Render the model in UV space into the offscreen mask texture
            D3D12_VIEWPORT vp = { 0.0f, 0.0f, (float)TEX_SIZE, (float)TEX_SIZE, 0.0f, 1.0f };
            D3D12_RECT     sr = { 0, 0, TEX_SIZE, TEX_SIZE };
            commandList->RSSetViewports(1, &vp);
            commandList->RSSetScissorRects(1, &sr);

            CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(maskRtvHeap->GetCPUDescriptorHandleForHeapStart());
            commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

            const float black[] = { 0.0f, 0.0f, 0.0f, 0.0f };
            commandList->ClearRenderTargetView(rtvHandle, black, 0, nullptr);

            shadedMesh->Draw(commandList.Get());

            // Transition to COPY_SOURCE then copy to CPU-readable readback buffer
            commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
                maskTexture.Get(),
                D3D12_RESOURCE_STATE_RENDER_TARGET,
                D3D12_RESOURCE_STATE_COPY_SOURCE));

            D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
            srcLoc.pResource       = maskTexture.Get();
            srcLoc.Type            = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            srcLoc.SubresourceIndex = 0;

            D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
            dstLoc.pResource       = readbackBuffer.Get();
            dstLoc.Type            = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            dstLoc.PlacedFootprint = readbackFootprint;

            commandList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

            maskRendered = true;
        }

        // Minimal swap chain frame: clear to a dark background and present
        commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            renderTargets[swapChainBackBufferIndex].Get(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET));

        CD3DX12_CPU_DESCRIPTOR_HANDLE swapRtv(
            rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
            swapChainBackBufferIndex,
            rtvDescriptorHandleIncrementSize);

        const float gray[] = { 0.1f, 0.1f, 0.1f, 1.0f };
        commandList->ClearRenderTargetView(swapRtv, gray, 0, nullptr);

        commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            renderTargets[swapChainBackBufferIndex].Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT));

        DX_API("Failed to close command list")
            commandList->Close();
    }

    virtual void CreateResources() override {
        Egg::SimpleApp::CreateResources();

        // Fixed-size offscreen render target for the UV mask
        D3D12_RESOURCE_DESC maskDesc = CD3DX12_RESOURCE_DESC::Tex2D(
            DXGI_FORMAT_R8G8B8A8_UNORM, TEX_SIZE, TEX_SIZE, 1, 1, 1, 0,
            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

        D3D12_CLEAR_VALUE clearValue = {};
        clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

        DX_API("Failed to create mask render target")
            device->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                D3D12_HEAP_FLAG_NONE,
                &maskDesc,
                D3D12_RESOURCE_STATE_RENDER_TARGET,
                &clearValue,
                IID_PPV_ARGS(maskTexture.GetAddressOf()));

        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = 1;
        rtvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        DX_API("Failed to create mask RTV heap")
            device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(maskRtvHeap.GetAddressOf()));

        device->CreateRenderTargetView(maskTexture.Get(), nullptr,
            maskRtvHeap->GetCPUDescriptorHandleForHeapStart());

        device->GetCopyableFootprints(&maskDesc, 0, 1, 0,
            &readbackFootprint, &readbackNumRows, &readbackRowSize, &readbackTotalSize);

        DX_API("Failed to create readback buffer")
            device->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
                D3D12_HEAP_FLAG_NONE,
                &CD3DX12_RESOURCE_DESC::Buffer(readbackTotalSize),
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                IID_PPV_ARGS(readbackBuffer.GetAddressOf()));
    }

    virtual void ReleaseResources() override {
        maskTexture.Reset();
        maskRtvHeap.Reset();
        readbackBuffer.Reset();
        Egg::SimpleApp::ReleaseResources();
    }

    virtual void LoadAssets() override {
        com_ptr<ID3DBlob> vs = Egg::Shader::LoadCso("Shaders/uvMaskVS.cso");
        com_ptr<ID3DBlob> ps = Egg::Shader::LoadCso("Shaders/uvMaskPS.cso");
        com_ptr<ID3D12RootSignature> rootSig = Egg::Shader::LoadRootSignature(device.Get(), vs.Get());

        Egg::Mesh::Material::P material = Egg::Mesh::Material::Create();
        material->SetRootSignature(rootSig);
        material->SetVertexShader(vs);
        material->SetPixelShader(ps);
        // Depth testing disabled by default in Material ctor

        Egg::Mesh::Geometry::P geometry = Egg::Importer::ImportSimpleObj(device.Get(), "kachu.fbx");

        shadedMesh = Egg::Mesh::Shaded::Create(psoManager, material, geometry);
    }
};
