#pragma once

#include <Egg/SimpleApp.h>
#include <Egg/Importer.h>
#include <Egg/ConstantBuffer.hpp>
#include <Egg/TextureCube.h>
#include <Egg/Cam/FirstPerson.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "ConstantBufferTypes.h"

using namespace Egg::Math;

typedef CD3DX12_PIPELINE_STATE_STREAM_SUBOBJECT<
    D3D12_SHADER_BYTECODE,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS>
    CD3DX12_PIPELINE_STATE_STREAM_MS;

typedef CD3DX12_PIPELINE_STATE_STREAM_SUBOBJECT<
    D3D12_SHADER_BYTECODE,
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS>
    CD3DX12_PIPELINE_STATE_STREAM_AS;

class mPhongTessellationApp : public Egg::SimpleApp {
protected:
    // Triangle mesh (torus) — Phong tessellation over triangles
    com_ptr<ID3D12PipelineState>  meshShaderPso;
    com_ptr<ID3D12RootSignature>  rootSig;
    com_ptr<ID3D12Resource>       vertexBuffer;
    com_ptr<ID3D12Resource>       indexBuffer;
    unsigned int                  numTriangles = 0;

    // Quad mesh (ico4) — bilinear Phong tessellation over quads
    com_ptr<ID3D12PipelineState>  quadMeshShaderPso;
    com_ptr<ID3D12RootSignature>  quadRootSig;
    com_ptr<ID3D12Resource>       quadVertexBuffer;
    com_ptr<ID3D12Resource>       quadIndexBuffer;
    unsigned int                  numQuads = 0;

    // SRV heap layout (6 slots):
    //   0=triVtx  1=triIdx  2=envMap  3=quadVtx  4=quadIdx  5=envMap(quad)
    com_ptr<ID3D12DescriptorHeap> srvHeap;

    Egg::ConstantBuffer<PerObjectCb> perObjectCb;   // torus
    Egg::ConstantBuffer<PerObjectCb> perObjectCb2;  // ico4
    Egg::ConstantBuffer<PerFrameCb>  perFrameCb;

    Egg::TextureCube              envMap;
    Egg::Cam::FirstPerson::P      camera;
    com_ptr<ID3D12GraphicsCommandList6> commandList6;

    // ---------------------------------------------------------------------------
    // Helpers
    // ---------------------------------------------------------------------------

    void LoadMesh() {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(
            //"../Media/cube3.fbx",
            "../Media/ico3.fbx",
            aiProcess_Triangulate | aiProcess_GenNormals);
        ASSERT(scene && scene->HasMeshes(),
               "Failed to load cube3.fbx: %s", importer.GetErrorString());

        const aiMesh* mesh = scene->mMeshes[0];
        struct CpuVertex { float px, py, pz, nx, ny, nz; };

        std::vector<CpuVertex> cpuVerts(mesh->mNumVertices);
        for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
            cpuVerts[i] = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z,
                            mesh->mNormals[i].x,  mesh->mNormals[i].y,  mesh->mNormals[i].z };

        std::vector<unsigned int> cpuIdx;
        cpuIdx.reserve(mesh->mNumFaces * 3);
        for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
            cpuIdx.push_back(mesh->mFaces[f].mIndices[0]);
            cpuIdx.push_back(mesh->mFaces[f].mIndices[1]);
            cpuIdx.push_back(mesh->mFaces[f].mIndices[2]);
        }
        numTriangles = mesh->mNumFaces;

        UploadStructuredBuffers(cpuVerts, cpuIdx, vertexBuffer, indexBuffer, 0, 1);
    }

    void LoadQuadMesh() {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(
            //"../Media/cube4.fbx",
            "../Media/ico4.fbx",
            aiProcess_GenSmoothNormals);  // no Triangulate — keep quads
        ASSERT(scene && scene->HasMeshes(),
               "Failed to load cube4.fbx: %s", importer.GetErrorString());

        const aiMesh* mesh = scene->mMeshes[0];
        struct CpuVertex { float px, py, pz, nx, ny, nz; };

        std::vector<CpuVertex> cpuVerts(mesh->mNumVertices);
        for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
            cpuVerts[i] = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z,
                            mesh->mNormals[i].x,  mesh->mNormals[i].y,  mesh->mNormals[i].z };

        std::vector<unsigned int> cpuIdx;
        cpuIdx.reserve(mesh->mNumFaces * 4);
        for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
            if (mesh->mFaces[f].mNumIndices == 4) {
                for (int k = 0; k < 4; ++k)
                    cpuIdx.push_back(mesh->mFaces[f].mIndices[k]);
                ++numQuads;
            }
        }
        ASSERT(numQuads > 0, "ico4.fbx contains no quad faces");

        UploadStructuredBuffers(cpuVerts, cpuIdx, quadVertexBuffer, quadIndexBuffer, 3, 4);
    }

    // Upload vertex + index structured buffers and create their SRVs at given heap slots.
    template<typename VtxT>
    void UploadStructuredBuffers(
        const std::vector<VtxT>& cpuVerts,
        const std::vector<unsigned int>& cpuIdx,
        com_ptr<ID3D12Resource>& outVtxBuf,
        com_ptr<ID3D12Resource>& outIdxBuf,
        UINT vtxSlot, UINT idxSlot)
    {
        auto upload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        UINT inc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        void* mapped = nullptr;
        CD3DX12_RANGE noRead(0, 0);

        size_t vtxBytes = cpuVerts.size() * sizeof(VtxT);
        DX_API("Failed to create vertex buffer")
            device->CreateCommittedResource(
                &upload, D3D12_HEAP_FLAG_NONE,
                &CD3DX12_RESOURCE_DESC::Buffer(vtxBytes),
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                IID_PPV_ARGS(outVtxBuf.GetAddressOf()));
        outVtxBuf->Map(0, &noRead, &mapped);
        memcpy(mapped, cpuVerts.data(), vtxBytes);
        outVtxBuf->Unmap(0, nullptr);

        size_t idxBytes = cpuIdx.size() * sizeof(unsigned int);
        DX_API("Failed to create index buffer")
            device->CreateCommittedResource(
                &upload, D3D12_HEAP_FLAG_NONE,
                &CD3DX12_RESOURCE_DESC::Buffer(idxBytes),
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                IID_PPV_ARGS(outIdxBuf.GetAddressOf()));
        outIdxBuf->Map(0, &noRead, &mapped);
        memcpy(mapped, cpuIdx.data(), idxBytes);
        outIdxBuf->Unmap(0, nullptr);

        D3D12_SHADER_RESOURCE_VIEW_DESC vtxSrv = {};
        vtxSrv.ViewDimension              = D3D12_SRV_DIMENSION_BUFFER;
        vtxSrv.Shader4ComponentMapping    = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        vtxSrv.Format                     = DXGI_FORMAT_UNKNOWN;
        vtxSrv.Buffer.NumElements         = (UINT)cpuVerts.size();
        vtxSrv.Buffer.StructureByteStride = sizeof(VtxT);
        device->CreateShaderResourceView(outVtxBuf.Get(), &vtxSrv,
            CD3DX12_CPU_DESCRIPTOR_HANDLE(srvHeap->GetCPUDescriptorHandleForHeapStart(), vtxSlot, inc));

        D3D12_SHADER_RESOURCE_VIEW_DESC idxSrv = {};
        idxSrv.ViewDimension              = D3D12_SRV_DIMENSION_BUFFER;
        idxSrv.Shader4ComponentMapping    = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        idxSrv.Format                     = DXGI_FORMAT_UNKNOWN;
        idxSrv.Buffer.NumElements         = (UINT)cpuIdx.size();
        idxSrv.Buffer.StructureByteStride = sizeof(unsigned int);
        device->CreateShaderResourceView(outIdxBuf.Get(), &idxSrv,
            CD3DX12_CPU_DESCRIPTOR_HANDLE(srvHeap->GetCPUDescriptorHandleForHeapStart(), idxSlot, inc));
    }

    void UploadEnvMap() {
        DX_API("Failed to reset command allocator")
            commandAllocator->Reset();
        DX_API("Failed to reset command list")
            commandList->Reset(commandAllocator.Get(), nullptr);
        envMap.UploadResource(commandList.Get());
        DX_API("Failed to close command list")
            commandList->Close();
        ID3D12CommandList* cls[] = { commandList.Get() };
        commandQueue->ExecuteCommandLists(_countof(cls), cls);
        WaitForPreviousFrame();
        envMap.ReleaseUploadResources();
    }

    com_ptr<ID3D12PipelineState> BuildMeshShaderPso(
        com_ptr<ID3DBlob>& msBlob,
        com_ptr<ID3DBlob>& psBlob,
        com_ptr<ID3D12RootSignature>& outRootSig)
    {
        outRootSig = Egg::Shader::LoadRootSignature(device.Get(), msBlob.Get());

        struct PsoStream {
            CD3DX12_PIPELINE_STATE_STREAM_SUBOBJECT<
                ID3D12RootSignature*,
                D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE>  rootSig;
            CD3DX12_PIPELINE_STATE_STREAM_MS                         ms;
            CD3DX12_PIPELINE_STATE_STREAM_PS                         ps;
            CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC                 blend;
            CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER                 rasterizer;
            CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL              depthStencil;
            CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT       dsvFormat;
            CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS      rtvFormats;
            CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_DESC                sampleDesc;
            CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_MASK                sampleMask;
        } stream;

        stream.rootSig = outRootSig.Get();
        stream.ms      = CD3DX12_SHADER_BYTECODE(msBlob.Get());
        stream.ps      = CD3DX12_SHADER_BYTECODE(psBlob.Get());

        CD3DX12_RASTERIZER_DESC rastDesc(D3D12_DEFAULT);
        rastDesc.CullMode = D3D12_CULL_MODE_NONE;
        stream.rasterizer = rastDesc;

        CD3DX12_DEPTH_STENCIL_DESC dsDesc(D3D12_DEFAULT);
        dsDesc.DepthEnable    = TRUE;
        dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        dsDesc.DepthFunc      = D3D12_COMPARISON_FUNC_LESS;
        stream.depthStencil   = dsDesc;
        stream.dsvFormat      = DXGI_FORMAT_D32_FLOAT;

        D3D12_RT_FORMAT_ARRAY rtvFmts = {};
        rtvFmts.NumRenderTargets = 1;
        rtvFmts.RTFormats[0]     = DXGI_FORMAT_R8G8B8A8_UNORM;
        stream.rtvFormats        = rtvFmts;

        D3D12_PIPELINE_STATE_STREAM_DESC streamDesc = {};
        streamDesc.SizeInBytes                   = sizeof(PsoStream);
        streamDesc.pPipelineStateSubobjectStream = &stream;

        com_ptr<ID3D12Device2> device2;
        DX_API("Failed to query ID3D12Device2")
            device->QueryInterface(IID_PPV_ARGS(device2.GetAddressOf()));

        com_ptr<ID3D12PipelineState> pso;
        DX_API("Failed to create mesh shader PSO")
            device2->CreatePipelineState(&streamDesc, IID_PPV_ARGS(pso.GetAddressOf()));
        return pso;
    }

public:
    virtual void Update(float dt, float T) override {
        float4x4 modelMatrix = float4x4::Rotation(float3::UnitY, T * 0.4f);
        perObjectCb->modelMat        = modelMatrix;
        perObjectCb->modelMatInverse = modelMatrix.Invert();
        perObjectCb.Upload();

        float4x4 modelMatrix2 = 
            float4x4::Rotation(float3::UnitY, T * 0.4f) *
            float4x4::Translation(float3(3.0f, 0.0f, 0.0f))
                                 ;
        perObjectCb2->modelMat        = modelMatrix2;
        perObjectCb2->modelMatInverse = modelMatrix2.Invert();
        perObjectCb2.Upload();

        camera->Animate(dt);

        perFrameCb->viewProjMat       = camera->GetViewMatrix() * camera->GetProjMatrix();
        perFrameCb->rayDirMat         = camera->GetRayDirMatrix();
        perFrameCb->cameraPos         = float4(camera->GetEyePosition(), 1.0f);
        perFrameCb->lightPos          = float4(0.0f, 2.0f, 3.0f, 1.0f);
        perFrameCb->lightPowerDensity = float4(1.0f, 1.0f, 1.0f, 1.0f);
        perFrameCb->billboardSize     = float4(0.0f, 0.0f, 0.0f, 0.0f);
        perFrameCb.Upload();
    }

    virtual void PopulateCommandList() override {
        commandAllocator->Reset();
        commandList->Reset(commandAllocator.Get(), nullptr);

        commandList->RSSetViewports(1, &viewPort);
        commandList->RSSetScissorRects(1, &scissorRect);

        commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            renderTargets[swapChainBackBufferIndex].Get(),
            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

        CD3DX12_CPU_DESCRIPTOR_HANDLE rHandle(
            rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
            swapChainBackBufferIndex, rtvDescriptorHandleIncrementSize);
        CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(dsvHeap->GetCPUDescriptorHandleForHeapStart());

        commandList->OMSetRenderTargets(1, &rHandle, FALSE, &dsvHandle);

        const float clearColor[] = { 0.05f, 0.1f, 0.2f, 1.0f };
        commandList->ClearRenderTargetView(rHandle, clearColor, 0, nullptr);
        commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        ID3D12DescriptorHeap* heaps[] = { srvHeap.Get() };
        commandList->SetDescriptorHeaps(_countof(heaps), heaps);
        UINT inc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        // Draw torus (triangle mesh) — descriptor table starts at slot 0
        commandList->SetPipelineState(meshShaderPso.Get());
        commandList->SetGraphicsRootSignature(rootSig.Get());
        commandList->SetGraphicsRootConstantBufferView(0, perObjectCb.GetGPUVirtualAddress());
        commandList->SetGraphicsRootConstantBufferView(1, perFrameCb.GetGPUVirtualAddress());
        commandList->SetGraphicsRootDescriptorTable(2,
            CD3DX12_GPU_DESCRIPTOR_HANDLE(srvHeap->GetGPUDescriptorHandleForHeapStart(), 0, inc));
        commandList6->DispatchMesh(numTriangles, 1, 1);

        // Draw ico4 (quad mesh) — descriptor table starts at slot 3
        commandList->SetPipelineState(quadMeshShaderPso.Get());
        commandList->SetGraphicsRootSignature(quadRootSig.Get());
        commandList->SetGraphicsRootConstantBufferView(0, perObjectCb2.GetGPUVirtualAddress());
        commandList->SetGraphicsRootConstantBufferView(1, perFrameCb.GetGPUVirtualAddress());
        commandList->SetGraphicsRootDescriptorTable(2,
            CD3DX12_GPU_DESCRIPTOR_HANDLE(srvHeap->GetGPUDescriptorHandleForHeapStart(), 3, inc));
        commandList6->DispatchMesh(numQuads, 1, 1);

        commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            renderTargets[swapChainBackBufferIndex].Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

        DX_API("Failed to close command list")
            commandList->Close();
    }

    virtual void CreateResources() override {
        Egg::SimpleApp::CreateResources();

        DX_API("Failed to query ID3D12GraphicsCommandList6")
            commandList->QueryInterface(IID_PPV_ARGS(commandList6.GetAddressOf()));

        // 6 slots: triVtx(0), triIdx(1), envMap(2), quadVtx(3), quadIdx(4), envMap(5)
        D3D12_DESCRIPTOR_HEAP_DESC dhd = {};
        dhd.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        dhd.NumDescriptors = 6;
        dhd.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        DX_API("Failed to create SRV descriptor heap")
            device->CreateDescriptorHeap(&dhd, IID_PPV_ARGS(srvHeap.GetAddressOf()));

        camera = Egg::Cam::FirstPerson::Create();
    }

    virtual void ReleaseResources() override {
        quadMeshShaderPso.Reset();
        quadRootSig.Reset();
        quadVertexBuffer.Reset();
        quadIndexBuffer.Reset();
        srvHeap.Reset();
        commandList6.Reset();
        Egg::SimpleApp::ReleaseResources();
    }

    virtual void LoadAssets() override {
        perObjectCb.CreateResources(device.Get());
        perObjectCb2.CreateResources(device.Get());
        perFrameCb.CreateResources(device.Get());

        // Camera centred between the two objects (torus at origin, ico4 at x=3)
        camera->SetView(float3(1.5f, 1.5f, -5.0f), float3(0.0f, 0.0f, 1.0f));
        camera->SetProj(1.2f, aspectRatio, 0.1f, 200.0f);
        camera->SetSpeed(3.0f);

        LoadMesh();
        LoadQuadMesh();

        envMap = Egg::Importer::ImportTextureCube(device.Get(), "cloudynoon.dds");
        UploadEnvMap();
        envMap.CreateSRV(device.Get(), srvHeap.Get(), 2);  // for torus draw
        envMap.CreateSRV(device.Get(), srvHeap.Get(), 5);  // for ico4 draw

        com_ptr<ID3DBlob> triMs  = Egg::Shader::LoadCso("Shaders/PhongTessMS.cso");
        com_ptr<ID3DBlob> quadMs = Egg::Shader::LoadCso("Shaders/PhongTessQuadMS.cso");
        com_ptr<ID3DBlob> ps     = Egg::Shader::LoadCso("Shaders/PhongTessPS.cso");

        meshShaderPso     = BuildMeshShaderPso(triMs,  ps, rootSig);
        quadMeshShaderPso = BuildMeshShaderPso(quadMs, ps, quadRootSig);
    }

    virtual void Resize(int width, int height) override {
        Egg::SimpleApp::Resize(width, height);
        if (camera)
            camera->SetAspect((float)width / (float)height);
    }

    void ProcessMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override {
        camera->ProcessMessage(hWnd, uMsg, wParam, lParam);
    }
};
