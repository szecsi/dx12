#pragma once
#include "Egg/Common.h"
#include <Egg/SimpleApp.h>
#include <Egg/Utility.h>
#include <Egg/Shader.h>
#include <Egg/ConstantBuffer.hpp>
#include <Egg/Cam/FirstPerson.h>
#include <Egg/Mesh/Geometry.h>

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>

#include "Shaders/EyeFrameCb.hlsli"
#include "Shaders/SeedGenCb.hlsli"
#include "Shaders/StrokeFrameCb.hlsli"
#include "Shaders/CompositeCb.hlsli"
#include "Shaders/CrossProjectCb.hlsli"

#include "Common/DescriptorAllocator.h"
#include "Common/GpuBuffer.h"

#include "MetaballSdf.h"
#include "MetaballCharacters.h"
#include "MarchingTetrahedra.h"
#include "CurvatureDirection.h"
#include "ImportedCharacter.h"
#include "StereoCamera.h"
#include "HatchVertex.h"

#include <vector>
#include <string>
#include <cmath>
#include <cstdint>

// GPU-side mirror of seedGenerateCS.hlsl / strokeVS.hlsl's SeedGpu struct --
// only sizeof() is used from C++ (to size the GpuBuffer's stride); the
// buffer's contents are written by the GPU (seedGenerateCS) and read by the
// GPU (strokeVS), never touched from the CPU side.
struct SeedGpu {
	Egg::Math::float2 screenPosPx;
	Egg::Math::float2 hatchDirScreen;
	float curvaturePx;
	float _pad0;
};

// k-StereoHatching: anaglyph stereo screen-space hatching over a handful of
// procedurally-generated smooth "metaball" characters. Scaffolding (Win32/
// device/swapchain boilerplate, SimpleApp subclassing shape, ImGui init/
// shutdown/BuildImGui pattern) follows g-Distance/g-Aequor's established
// conventions; none of their actual buffers, constant buffers, or shaders
// are reused. See the approved plan (ok-create-a-new-ethereal-bonbon.md)
// for the full design rationale.
class StereoHatchingApp : public Egg::SimpleApp {

	static constexpr uint32_t MaxSeeds = 8000; // fixed buffer capacity; GUI "Max Seeds" only lowers the runtime accept-cap
	static constexpr uint32_t StrokeSegments = 8; // must match strokeVS.hlsl's STROKE_SEGMENTS -- sub-quads chained along the arc so curvature is actually tessellated, not just evaluated at the two endpoints

	struct EyeResources {
		com_ptr<ID3D12Resource> sceneColor, luminance, directionMap, strokeMask, eyeDepth, crossMask;
		D3D12_CPU_DESCRIPTOR_HANDLE rtvSceneColor{}, rtvLuminance{}, rtvDirectionMap{}, rtvStrokeMask{}, rtvCrossMask{}, dsv{};
		D3D12_GPU_DESCRIPTOR_HANDLE srvTableLumDir{}; // {luminance, directionMap} SRV pair, for seedGenerateCS
		GpuBuffer::P seedBuffer, seedCounterBuffer;
		Egg::ConstantBuffer<EyeFrameCb> frameCb;
		Egg::ConstantBuffer<SeedGenCb> seedGenCb;
		Egg::ConstantBuffer<StrokeFrameCb> strokeFrameCb;
		Egg::ConstantBuffer<CrossProjectCb> crossProjectCb; // filled with THIS eye's own + the OTHER eye's viewProj
		uint32_t hashSeed = 0;
	};

	enum class RenderMode { Unsynced = 0, CrossProjected = 1 };
	enum class SceneType { Metaballs = 0, LsdMolecule = 1, NoseyKnight = 2 };

	EyeResources eyes[2]; // 0 = left, 1 = right
	D3D12_GPU_DESCRIPTOR_HANDLE compositeTableGpu{}; // {leftStrokeMask, rightStrokeMask, leftLuminance, rightLuminance, leftCrossMask, rightCrossMask} SRV sextuple, for compositePS
	// crossInputTableGpu[e] = {otherStrokeMask, otherDepth} SRV pair to bind
	// when rendering eye e's OWN cross-project pass (i.e. it points at the
	// OPPOSING eye's textures).
	D3D12_GPU_DESCRIPTOR_HANDLE crossInputTableGpu[2]{};

	DescriptorAllocator::P mainAlloc;
	com_ptr<ID3D12DescriptorHeap> eyeRtvHeap, eyeDsvHeap;
	com_ptr<ID3D12DescriptorHeap> imguiSrvHeap;

	Egg::Cam::FirstPerson::P headCamera;

	std::vector<Hatch::Character> characters;
	std::vector<Egg::Mesh::IndexedGeometry::P> characterGeometry;

	Egg::ConstantBuffer<CompositeCb> compositeCb;

	com_ptr<ID3D12RootSignature> hatchGeometryRootSig;
	com_ptr<ID3D12PipelineState> hatchGeometryPso;
	com_ptr<ID3D12RootSignature> clearSeedCounterRootSig;
	com_ptr<ID3D12PipelineState> clearSeedCounterPso;
	com_ptr<ID3D12RootSignature> seedGenRootSig;
	com_ptr<ID3D12PipelineState> seedGenPso;
	com_ptr<ID3D12RootSignature> strokeRootSig;
	com_ptr<ID3D12PipelineState> strokePso;
	com_ptr<ID3D12RootSignature> compositeRootSig;
	com_ptr<ID3D12PipelineState> compositePso;
	com_ptr<ID3D12RootSignature> crossProjectRootSig;
	com_ptr<ID3D12PipelineState> crossProjectPso;

	// -- GUI-tunable parameters --
	float eyeSeparation = 0.065f;
	float convergenceDistance = 4.5f;
	float fov = 0.9f;
	float nearPlane = 0.1f;
	float farPlane = 50.0f;

	float lightAzimuthDeg = 45.0f;
	float lightElevationDeg = 55.0f;
	float lightIntensity = 1.5f;
	float ambient = 0.15f;
	float kd = 1.0f;
	float ks = 0.5f;
	float specExponent = 32.0f;

	int   seedCellSizePx = 14;
	float densityGamma = 1.5f;
	float seedDensityScale = 1.0f;
	int   maxSeedsSetting = 3000;

	float strokeLengthPx = 26.0f;
	float strokeWidthPx = 3.0f;
	float curvatureBendScale = 1.0f;

	float paperColorRgb[3] = { 1.0f, 1.0f, 1.0f };
	float leftInkStrength = 1.0f;
	float rightInkStrength = 1.0f;
	bool  showLuminanceMap = false;

	RenderMode renderMode = RenderMode::Unsynced;
	bool  showUnprojectedHatching = true;
	bool  showCrossProjectedHatching = true;
	static constexpr float CrossProjectDepthEpsilon = 0.002f; // NDC-z tolerance for "is this the same surface point the other eye sees"

	SceneType currentScene = SceneType::Metaballs;
	int   tessellationGridRes = 56;
	float curvatureBlendRadius = 0.25f;
	bool  needsRebuildCharacters = false;

	// -- resource-creation helpers --

	com_ptr<ID3D12Resource> CreateColorTarget(UINT w, UINT h, DXGI_FORMAT fmt, D3D12_RESOURCE_STATES initState, const float clearColor[4], const wchar_t* name) {
		D3D12_CLEAR_VALUE cv = {};
		cv.Format = fmt;
		cv.Color[0] = clearColor[0]; cv.Color[1] = clearColor[1]; cv.Color[2] = clearColor[2]; cv.Color[3] = clearColor[3];
		com_ptr<ID3D12Resource> res;
		DX_API("create color target")
			device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Tex2D(fmt, w, h, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),
				initState, &cv, IID_PPV_ARGS(res.GetAddressOf()));
		res->SetName(name);
		return res;
	}

	// R32_TYPELESS (not D32_FLOAT directly) so the SAME resource can be
	// viewed both as a D32_FLOAT DSV (normal depth test/write) and an
	// R32_FLOAT SRV (crossProjectPS.hlsl reading the OPPOSING eye's depth
	// to reject reprojected points it doesn't actually see) -- D3D12
	// requires the typeless-resource trick for that dual use.
	com_ptr<ID3D12Resource> CreateDepthTarget(UINT w, UINT h, const wchar_t* name) {
		D3D12_CLEAR_VALUE cv = {};
		cv.Format = DXGI_FORMAT_D32_FLOAT;
		cv.DepthStencil.Depth = 1.0f;
		cv.DepthStencil.Stencil = 0;
		com_ptr<ID3D12Resource> res;
		DX_API("create depth target")
			device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_TYPELESS, w, h, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
				D3D12_RESOURCE_STATE_DEPTH_WRITE, &cv, IID_PPV_ARGS(res.GetAddressOf()));
		res->SetName(name);
		return res;
	}

	void CreateTex2DSrv(ID3D12Resource* res, DXGI_FORMAT fmt, D3D12_CPU_DESCRIPTOR_HANDLE cpu) {
		D3D12_SHADER_RESOURCE_VIEW_DESC d = {};
		d.Format = fmt;
		d.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		d.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		d.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(res, &d, cpu);
	}

	void RebuildCharacters() {
		characterGeometry.clear();

		// Imported-mesh scene: one shared geometry (loaded once from FBX,
		// no marching-tetrahedra/SDF involved) drawn at several world
		// offsets -- entirely bypasses the procedural per-character loop
		// below, which needs a CharacterSdf to tessellate.
		if (currentScene == SceneType::NoseyKnight) {
			characters = Hatch::BuildNoseyKnightPlacements();
			Egg::Mesh::IndexedGeometry::P geom = Hatch::LoadMeshCharacter(device.Get(), "nosey-knight.fbx");
			for (size_t i = 0; i < characters.size(); ++i)
				characterGeometry.push_back(geom);
			return;
		}

		characters = (currentScene == SceneType::LsdMolecule)
			? Hatch::BuildLsdMolecule(curvatureBlendRadius)
			: Hatch::BuildCharacters(curvatureBlendRadius);
		for (auto& ch : characters) {
			Hatch::AABB bbox = Hatch::ComputeLocalBounds(ch.sdf);
			std::vector<Hatch::HatchVertex> verts;
			std::vector<uint32_t> indices;
			Hatch::TessellateCharacter(ch.sdf, bbox, (uint32_t)tessellationGridRes, verts, indices);
			if (verts.empty() || indices.empty()) continue;
			Hatch::ComputeCurvatureDirections(ch.sdf, verts);

			auto geom = Egg::Mesh::IndexedGeometry::Create(device.Get(),
				verts.data(), (unsigned int)(verts.size() * sizeof(Hatch::HatchVertex)), (unsigned int)sizeof(Hatch::HatchVertex),
				indices.data(), (unsigned int)(indices.size() * sizeof(uint32_t)), DXGI_FORMAT_R32_UINT);
			characterGeometry.push_back(geom);
		}
	}

	void CreateHatchGeometryPso() {
		com_ptr<ID3DBlob> vs = Egg::Shader::LoadCso("Shaders/hatchGeometryVS.cso");
		com_ptr<ID3DBlob> ps = Egg::Shader::LoadCso("Shaders/hatchGeometryPS.cso");
		hatchGeometryRootSig = Egg::Shader::LoadRootSignature(device.Get(), vs.Get());

		D3D12_INPUT_ELEMENT_DESC layout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 1, DXGI_FORMAT_R32_FLOAT,       0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = hatchGeometryRootSig.Get();
		psoDesc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
		psoDesc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // marching-tetrahedra winding not guaranteed consistent; normals come from the analytic SDF gradient, not triangle winding
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = TRUE;
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.InputLayout = { layout, _countof(layout) };
		psoDesc.NumRenderTargets = 3;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R11G11B10_FLOAT;
		psoDesc.RTVFormats[1] = DXGI_FORMAT_R16_FLOAT;
		psoDesc.RTVFormats[2] = DXGI_FORMAT_R16G16B16A16_FLOAT;
		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.SampleDesc.Count = 1;
		DX_API("create hatch geometry PSO")
			device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(hatchGeometryPso.GetAddressOf()));
	}

	void CreateComputePsos() {
		{
			com_ptr<ID3DBlob> cs = Egg::Shader::LoadCso("Shaders/clearSeedCounterCS.cso");
			clearSeedCounterRootSig = Egg::Shader::LoadRootSignature(device.Get(), cs.Get());
			D3D12_COMPUTE_PIPELINE_STATE_DESC d = {};
			d.pRootSignature = clearSeedCounterRootSig.Get();
			d.CS = { cs->GetBufferPointer(), cs->GetBufferSize() };
			DX_API("create clear seed counter PSO")
				device->CreateComputePipelineState(&d, IID_PPV_ARGS(clearSeedCounterPso.GetAddressOf()));
		}
		{
			com_ptr<ID3DBlob> cs = Egg::Shader::LoadCso("Shaders/seedGenerateCS.cso");
			seedGenRootSig = Egg::Shader::LoadRootSignature(device.Get(), cs.Get());
			D3D12_COMPUTE_PIPELINE_STATE_DESC d = {};
			d.pRootSignature = seedGenRootSig.Get();
			d.CS = { cs->GetBufferPointer(), cs->GetBufferSize() };
			DX_API("create seed generate PSO")
				device->CreateComputePipelineState(&d, IID_PPV_ARGS(seedGenPso.GetAddressOf()));
		}
	}

	void CreateStrokePso() {
		com_ptr<ID3DBlob> vs = Egg::Shader::LoadCso("Shaders/strokeVS.cso");
		com_ptr<ID3DBlob> ps = Egg::Shader::LoadCso("Shaders/strokePS.cso");
		strokeRootSig = Egg::Shader::LoadRootSignature(device.Get(), vs.Get());

		// strokeMask is single-channel (R8_UNORM): accumulate ink coverage
		// additively (see strokePS.hlsl's header comment) rather than
		// alpha-over compositing, which needs an alpha channel this target
		// doesn't have.
		D3D12_BLEND_DESC additiveBlend = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		additiveBlend.RenderTarget[0].BlendEnable    = TRUE;
		additiveBlend.RenderTarget[0].SrcBlend       = D3D12_BLEND_ONE;
		additiveBlend.RenderTarget[0].DestBlend      = D3D12_BLEND_ONE;
		additiveBlend.RenderTarget[0].BlendOp        = D3D12_BLEND_OP_ADD;
		additiveBlend.RenderTarget[0].SrcBlendAlpha  = D3D12_BLEND_ONE;
		additiveBlend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
		additiveBlend.RenderTarget[0].BlendOpAlpha   = D3D12_BLEND_OP_ADD;

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = strokeRootSig.Get();
		psoDesc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
		psoDesc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = FALSE; // seeds only ever spawn where a character was rasterized -- silhouette occlusion is already baked into seed placement
		psoDesc.BlendState = additiveBlend;
		D3D12_INPUT_LAYOUT_DESC emptyLayout = { nullptr, 0 };
		psoDesc.InputLayout = emptyLayout;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8_UNORM;
		psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.SampleDesc.Count = 1;
		DX_API("create stroke PSO")
			device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(strokePso.GetAddressOf()));
	}

	void CreateCompositePso() {
		com_ptr<ID3DBlob> vs = Egg::Shader::LoadCso("Shaders/compositeVS.cso");
		com_ptr<ID3DBlob> ps = Egg::Shader::LoadCso("Shaders/compositePS.cso");
		compositeRootSig = Egg::Shader::LoadRootSignature(device.Get(), vs.Get());

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = compositeRootSig.Get();
		psoDesc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
		psoDesc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = FALSE;
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		D3D12_INPUT_LAYOUT_DESC emptyLayout = { nullptr, 0 };
		psoDesc.InputLayout = emptyLayout;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.SampleDesc.Count = 1;
		DX_API("create composite PSO")
			device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(compositePso.GetAddressOf()));
	}

	void CreateCrossProjectPso() {
		com_ptr<ID3DBlob> vs = Egg::Shader::LoadCso("Shaders/crossProjectVS.cso");
		com_ptr<ID3DBlob> ps = Egg::Shader::LoadCso("Shaders/crossProjectPS.cso");
		crossProjectRootSig = Egg::Shader::LoadRootSignature(device.Get(), vs.Get());

		D3D12_INPUT_ELEMENT_DESC layout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 1, DXGI_FORMAT_R32_FLOAT,       0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = crossProjectRootSig.Get();
		psoDesc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
		psoDesc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = TRUE;
		// Re-tests against THIS eye's own depth, already fully written by
		// Pass A's hatchGeometryPso earlier in the SAME frame -- same
		// vertices/transforms, so the values compare bit-for-bit equal;
		// LESS_EQUAL (not LESS) with WriteMask ZERO keeps exactly the
		// already-established visible surface without re-writing it.
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.InputLayout = { layout, _countof(layout) };
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8_UNORM;
		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.SampleDesc.Count = 1;
		DX_API("create cross-project PSO")
			device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(crossProjectPso.GetAddressOf()));
	}

	void BuildImGui() {
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::SetNextWindowSize(ImVec2(320, 0), ImGuiCond_FirstUseEver);
		ImGui::Begin("k-StereoHatching Controls");

		if (ImGui::CollapsingHeader("Scene", ImGuiTreeNodeFlags_DefaultOpen)) {
			static const char* sceneItems[] = { "Metaball Characters", "LSD Molecule", "Nosey Knight" };
			int sceneIdx = (int)currentScene;
			if (ImGui::Combo("Active Scene", &sceneIdx, sceneItems, IM_ARRAYSIZE(sceneItems))) {
				currentScene = (SceneType)sceneIdx;
				needsRebuildCharacters = true;
			}
		}

		if (ImGui::CollapsingHeader("Stereo", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::SliderFloat("Eye Separation", &eyeSeparation, 0.0f, 0.3f);
			ImGui::SliderFloat("Convergence Distance", &convergenceDistance, 0.5f, 10.0f);
		}

		if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::SliderFloat("Light Azimuth", &lightAzimuthDeg, 0.0f, 360.0f);
			ImGui::SliderFloat("Light Elevation", &lightElevationDeg, -10.0f, 89.0f);
			ImGui::SliderFloat("Light Intensity", &lightIntensity, 0.0f, 5.0f);
			ImGui::SliderFloat("Ambient", &ambient, 0.0f, 1.0f);
			ImGui::SliderFloat("Kd (diffuse)", &kd, 0.0f, 2.0f);
			ImGui::SliderFloat("Ks (specular)", &ks, 0.0f, 2.0f);
			ImGui::SliderFloat("Specular Exponent", &specExponent, 1.0f, 128.0f, "%.0f", ImGuiSliderFlags_Logarithmic);
		}

		if (ImGui::CollapsingHeader("Seeds", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::SliderInt("Seed Cell Size (px)", &seedCellSizePx, 8, 40);
			ImGui::SliderFloat("Density Gamma", &densityGamma, 0.25f, 4.0f);
			ImGui::SliderFloat("Seed Density Scale", &seedDensityScale, 0.0f, 3.0f);
			ImGui::SliderInt("Max Seeds", &maxSeedsSetting, 500, (int)MaxSeeds);
		}

		if (ImGui::CollapsingHeader("Strokes", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::SliderFloat("Stroke Length (px)", &strokeLengthPx, 6.0f, 60.0f);
			ImGui::SliderFloat("Stroke Width (px)", &strokeWidthPx, 1.0f, 8.0f);
			ImGui::SliderFloat("Curvature Bend Scale", &curvatureBendScale, 0.0f, 3.0f);
			ImGui::SameLine();
			ImGui::TextDisabled("(0 = straight strokes)");
		}

		if (ImGui::CollapsingHeader("Composite", ImGuiTreeNodeFlags_DefaultOpen)) {
			static const char* modeItems[] = { "Unsynced", "Cross-Projected" };
			int modeIdx = (int)renderMode;
			if (ImGui::Combo("Render Mode", &modeIdx, modeItems, IM_ARRAYSIZE(modeItems))) renderMode = (RenderMode)modeIdx;
			ImGui::SameLine();
			ImGui::TextDisabled("(?)");
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(
					"Unsynced: each eye shows only its own independently-\n"
					"seeded hatching (as before).\n"
					"Cross-Projected: each eye's surface also (optionally)\n"
					"shows the OTHER eye's hatching, reprojected along the\n"
					"actual 3D surface and drawn in this eye's own color --\n"
					"a right-eye (cyan) stroke can appear as red in the left\n"
					"eye, and vice versa, wherever the surface allows the\n"
					"mapping (in view and not occluded from the other eye).");
			}

			if (renderMode == RenderMode::CrossProjected) {
				ImGui::Checkbox("Show Unprojected Hatching (own eye)", &showUnprojectedHatching);
				ImGui::Checkbox("Show Cross-Projected Hatching (other eye)", &showCrossProjectedHatching);
			}

			ImGui::ColorEdit3("Paper Color", paperColorRgb);
			ImGui::SliderFloat("Left Ink Strength", &leftInkStrength, 0.0f, 1.0f);
			ImGui::SliderFloat("Right Ink Strength", &rightInkStrength, 0.0f, 1.0f);
			ImGui::Checkbox("Show Luminance Map (below hatching)", &showLuminanceMap);
		}

		if (ImGui::CollapsingHeader("Character Rebuild")) {
			ImGui::SliderInt("Tessellation Grid Resolution", &tessellationGridRes, 16, 128);
			ImGui::SliderFloat("Curvature Blend Radius", &curvatureBlendRadius, 0.05f, 0.6f);
			if (ImGui::Button("Rebuild Characters")) needsRebuildCharacters = true;
			ImGui::SameLine();
			ImGui::TextDisabled("('R' key)");
		}

		ImGui::Separator();
		ImGui::Text("%.1f FPS", ImGui::GetIO().Framerate);
		ImGui::End();

		ImGui::Render();
		ID3D12DescriptorHeap* imguiHeaps[] = { imguiSrvHeap.Get() };
		commandList->SetDescriptorHeaps(1, imguiHeaps);
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.Get());
	}

public:
	StereoHatchingApp() : SimpleApp() {}

	void InitImGui(HWND hwnd) {
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();

		ImGui_ImplWin32_Init(hwnd);

		uint dhIncrSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		ImGui_ImplDX12_InitInfo initInfo;
		initInfo.Device = device.Get();
		initInfo.CommandQueue = commandQueue.Get();
		initInfo.NumFramesInFlight = (int)swapChainBackBufferCount;
		initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		initInfo.SrvDescriptorHeap = imguiSrvHeap.Get();
		initInfo.LegacySingleSrvCpuDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(imguiSrvHeap->GetCPUDescriptorHandleForHeapStart(), 0, dhIncrSize);
		initInfo.LegacySingleSrvGpuDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(imguiSrvHeap->GetGPUDescriptorHandleForHeapStart(), 0, dhIncrSize);
		ImGui_ImplDX12_Init(&initInfo);
	}

	void ShutdownImGui() {
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}

	virtual void CreateResources() override {
		Egg::SimpleApp::CreateResources();

		D3D12_DESCRIPTOR_HEAP_DESC imguiHeapDesc = {};
		imguiHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		imguiHeapDesc.NumDescriptors = 1;
		imguiHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		DX_API("create imgui srv heap")
			device->CreateDescriptorHeap(&imguiHeapDesc, IID_PPV_ARGS(imguiSrvHeap.GetAddressOf()));

		compositeCb.CreateResources(device.Get());

		for (int e = 0; e < 2; ++e) {
			eyes[e].frameCb.CreateResources(device.Get());
			eyes[e].seedGenCb.CreateResources(device.Get());
			eyes[e].strokeFrameCb.CreateResources(device.Get());
			eyes[e].crossProjectCb.CreateResources(device.Get());
			eyes[e].hashSeed = (e == 0) ? 0x9E3779B1u : 0x85EBCA6Bu;

			eyes[e].seedBuffer = GpuBuffer::Create(device.Get(), MaxSeeds, (UINT)sizeof(SeedGpu),
				(e == 0) ? L"SeedBufferL" : L"SeedBufferR", D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_HEAP_TYPE_DEFAULT);
			eyes[e].seedCounterBuffer = GpuBuffer::Create(device.Get(), 1, (UINT)sizeof(UINT),
				(e == 0) ? L"SeedCounterL" : L"SeedCounterR", D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_HEAP_TYPE_DEFAULT);
		}

		CreateHatchGeometryPso();
		CreateComputePsos();
		CreateStrokePso();
		CreateCompositePso();
		CreateCrossProjectPso();
	}

	virtual void CreateSwapChainResources() override {
		Egg::SimpleApp::CreateSwapChainResources();
		if (headCamera) headCamera->SetAspect(aspectRatio);

		UINT w = (UINT)viewPort.Width, h = (UINT)viewPort.Height;
		if (w == 0 || h == 0) return;

		mainAlloc = DescriptorAllocator::Create(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 24, true);

		D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
		rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvDesc.NumDescriptors = 10;
		DX_API("create eye rtv heap")
			device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(eyeRtvHeap.ReleaseAndGetAddressOf()));
		UINT rtvInc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		D3D12_DESCRIPTOR_HEAP_DESC dsvDesc = {};
		dsvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		dsvDesc.NumDescriptors = 2;
		DX_API("create eye dsv heap")
			device->CreateDescriptorHeap(&dsvDesc, IID_PPV_ARGS(eyeDsvHeap.ReleaseAndGetAddressOf()));
		UINT dsvInc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

		const D3D12_RESOURCE_STATES ReadState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

		for (int e = 0; e < 2; ++e) {
			const wchar_t* eyeName = (e == 0) ? L"L" : L"R";

			const float sceneClear[4] = { 0.85f, 0.90f, 0.95f, 1.0f };
			eyes[e].sceneColor = CreateColorTarget(w, h, DXGI_FORMAT_R11G11B10_FLOAT, D3D12_RESOURCE_STATE_RENDER_TARGET, sceneClear, (std::wstring(L"SceneColor") + eyeName).c_str());

			const float lumClear[4] = { -1.0f, -1.0f, -1.0f, -1.0f };
			eyes[e].luminance = CreateColorTarget(w, h, DXGI_FORMAT_R16_FLOAT, ReadState, lumClear, (std::wstring(L"Luminance") + eyeName).c_str());

			const float dirClear[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			eyes[e].directionMap = CreateColorTarget(w, h, DXGI_FORMAT_R16G16B16A16_FLOAT, ReadState, dirClear, (std::wstring(L"DirectionMap") + eyeName).c_str());

			const float strokeClear[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			eyes[e].strokeMask = CreateColorTarget(w, h, DXGI_FORMAT_R8_UNORM, ReadState, strokeClear, (std::wstring(L"StrokeMask") + eyeName).c_str());

			eyes[e].crossMask = CreateColorTarget(w, h, DXGI_FORMAT_R8_UNORM, ReadState, strokeClear, (std::wstring(L"CrossMask") + eyeName).c_str());

			eyes[e].eyeDepth = CreateDepthTarget(w, h, (std::wstring(L"EyeDepth") + eyeName).c_str());

			eyes[e].rtvSceneColor    = CD3DX12_CPU_DESCRIPTOR_HANDLE(eyeRtvHeap->GetCPUDescriptorHandleForHeapStart(), e * 5 + 0, rtvInc);
			eyes[e].rtvLuminance     = CD3DX12_CPU_DESCRIPTOR_HANDLE(eyeRtvHeap->GetCPUDescriptorHandleForHeapStart(), e * 5 + 1, rtvInc);
			eyes[e].rtvDirectionMap  = CD3DX12_CPU_DESCRIPTOR_HANDLE(eyeRtvHeap->GetCPUDescriptorHandleForHeapStart(), e * 5 + 2, rtvInc);
			eyes[e].rtvStrokeMask    = CD3DX12_CPU_DESCRIPTOR_HANDLE(eyeRtvHeap->GetCPUDescriptorHandleForHeapStart(), e * 5 + 3, rtvInc);
			eyes[e].rtvCrossMask     = CD3DX12_CPU_DESCRIPTOR_HANDLE(eyeRtvHeap->GetCPUDescriptorHandleForHeapStart(), e * 5 + 4, rtvInc);

			device->CreateRenderTargetView(eyes[e].sceneColor.Get(), nullptr, eyes[e].rtvSceneColor);
			device->CreateRenderTargetView(eyes[e].luminance.Get(), nullptr, eyes[e].rtvLuminance);
			device->CreateRenderTargetView(eyes[e].directionMap.Get(), nullptr, eyes[e].rtvDirectionMap);
			device->CreateRenderTargetView(eyes[e].strokeMask.Get(), nullptr, eyes[e].rtvStrokeMask);
			device->CreateRenderTargetView(eyes[e].crossMask.Get(), nullptr, eyes[e].rtvCrossMask);

			eyes[e].dsv = CD3DX12_CPU_DESCRIPTOR_HANDLE(eyeDsvHeap->GetCPUDescriptorHandleForHeapStart(), e, dsvInc);
			D3D12_DEPTH_STENCIL_VIEW_DESC dsvViewDesc = {};
			dsvViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
			dsvViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			device->CreateDepthStencilView(eyes[e].eyeDepth.Get(), &dsvViewDesc, eyes[e].dsv);

			// Contiguous {luminance, directionMap} SRV pair for seedGenerateCS's table.
			UINT lumSlot = mainAlloc->Allocate();
			UINT dirSlot = mainAlloc->Allocate();
			CreateTex2DSrv(eyes[e].luminance.Get(), DXGI_FORMAT_R16_FLOAT, mainAlloc->GetCpuHandle(lumSlot));
			CreateTex2DSrv(eyes[e].directionMap.Get(), DXGI_FORMAT_R16G16B16A16_FLOAT, mainAlloc->GetCpuHandle(dirSlot));
			eyes[e].srvTableLumDir = mainAlloc->GetGpuHandle(lumSlot);
		}

		// Contiguous {leftStrokeMask, rightStrokeMask, leftLuminance,
		// rightLuminance, leftCrossMask, rightCrossMask} SRV sextuple for
		// compositePS's table -- the luminance pair is a SECOND set of SRVs
		// onto the same textures already viewed by srvTableLumDir above (a
		// resource can have multiple views), needed here in a different,
		// composite-specific contiguous order.
		UINT leftStrokeSlot = mainAlloc->Allocate();
		UINT rightStrokeSlot = mainAlloc->Allocate();
		UINT leftLumSlot = mainAlloc->Allocate();
		UINT rightLumSlot = mainAlloc->Allocate();
		UINT leftCrossSlot = mainAlloc->Allocate();
		UINT rightCrossSlot = mainAlloc->Allocate();
		CreateTex2DSrv(eyes[0].strokeMask.Get(), DXGI_FORMAT_R8_UNORM, mainAlloc->GetCpuHandle(leftStrokeSlot));
		CreateTex2DSrv(eyes[1].strokeMask.Get(), DXGI_FORMAT_R8_UNORM, mainAlloc->GetCpuHandle(rightStrokeSlot));
		CreateTex2DSrv(eyes[0].luminance.Get(), DXGI_FORMAT_R16_FLOAT, mainAlloc->GetCpuHandle(leftLumSlot));
		CreateTex2DSrv(eyes[1].luminance.Get(), DXGI_FORMAT_R16_FLOAT, mainAlloc->GetCpuHandle(rightLumSlot));
		CreateTex2DSrv(eyes[0].crossMask.Get(), DXGI_FORMAT_R8_UNORM, mainAlloc->GetCpuHandle(leftCrossSlot));
		CreateTex2DSrv(eyes[1].crossMask.Get(), DXGI_FORMAT_R8_UNORM, mainAlloc->GetCpuHandle(rightCrossSlot));
		compositeTableGpu = mainAlloc->GetGpuHandle(leftStrokeSlot);

		// Contiguous {otherStrokeMask, otherDepth} SRV pair per eye, for
		// crossProjectPS.hlsl -- eye e's OWN cross-project pass reads the
		// OPPOSING eye's finished hatching and depth, so crossInputTableGpu[e]
		// points at eye (1-e)'s textures. The depth SRV views the same
		// R32_TYPELESS resource CreateDepthTarget allocated, as R32_FLOAT
		// (its DSV, created above, views it as D32_FLOAT).
		for (int e = 0; e < 2; ++e) {
			int other = 1 - e;
			UINT otherStrokeSlot = mainAlloc->Allocate();
			UINT otherDepthSlot = mainAlloc->Allocate();
			CreateTex2DSrv(eyes[other].strokeMask.Get(), DXGI_FORMAT_R8_UNORM, mainAlloc->GetCpuHandle(otherStrokeSlot));
			CreateTex2DSrv(eyes[other].eyeDepth.Get(), DXGI_FORMAT_R32_FLOAT, mainAlloc->GetCpuHandle(otherDepthSlot));
			crossInputTableGpu[e] = mainAlloc->GetGpuHandle(otherStrokeSlot);
		}
	}

	virtual void LoadAssets() override {
		using namespace Egg::Math;

		headCamera = Egg::Cam::FirstPerson::Create();
		headCamera->SetProj(fov, aspectRatio, nearPlane, farPlane);
		headCamera->SetAspect(aspectRatio);
		headCamera->SetSpeed(1.0f);
		float3 target(0.0f, 0.9f, 0.0f);
		float3 eye = target + float3(0.0f, 0.5f, -4.6f);
		headCamera->SetView(eye, (target - eye).Normalize());

		RebuildCharacters();
	}

	virtual void Render() override {
		if (needsRebuildCharacters) {
			RebuildCharacters();
			needsRebuildCharacters = false;
		}

		PopulateCommandList();

		ID3D12CommandList* cLists[] = { commandList.Get() };
		commandQueue->ExecuteCommandLists(1, cLists);

		DX_API("Failed to present swap chain")
			swapChain->Present(1, 0);

		WaitForPreviousFrame();
	}

	virtual void PopulateCommandList() override {
		DX_API("reset command allocator") commandAllocator->Reset();
		DX_API("reset command list") commandList->Reset(commandAllocator.Get(), nullptr);

		ID3D12DescriptorHeap* mainHeaps[] = { mainAlloc->GetHeap() };
		commandList->SetDescriptorHeaps(1, mainHeaps);

		const D3D12_RESOURCE_STATES ReadState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

		for (int e = 0; e < 2; ++e) {
			EyeResources& eye = eyes[e];

			// -- Pass A: shading + direction G-buffer --
			{
				D3D12_RESOURCE_BARRIER toRt[2] = {
					CD3DX12_RESOURCE_BARRIER::Transition(eye.luminance.Get(), ReadState, D3D12_RESOURCE_STATE_RENDER_TARGET),
					CD3DX12_RESOURCE_BARRIER::Transition(eye.directionMap.Get(), ReadState, D3D12_RESOURCE_STATE_RENDER_TARGET),
				};
				commandList->ResourceBarrier(2, toRt);

				D3D12_CPU_DESCRIPTOR_HANDLE rtvs[3] = { eye.rtvSceneColor, eye.rtvLuminance, eye.rtvDirectionMap };
				commandList->OMSetRenderTargets(3, rtvs, FALSE, &eye.dsv);
				commandList->RSSetViewports(1, &viewPort);
				commandList->RSSetScissorRects(1, &scissorRect);

				const float sceneClear[4] = { 0.85f, 0.90f, 0.95f, 1.0f };
				const float lumClear[4] = { -1.0f, -1.0f, -1.0f, -1.0f };
				const float dirClear[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				commandList->ClearRenderTargetView(eye.rtvSceneColor, sceneClear, 0, nullptr);
				commandList->ClearRenderTargetView(eye.rtvLuminance, lumClear, 0, nullptr);
				commandList->ClearRenderTargetView(eye.rtvDirectionMap, dirClear, 0, nullptr);
				commandList->ClearDepthStencilView(eye.dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

				commandList->SetGraphicsRootSignature(hatchGeometryRootSig.Get());
				commandList->SetGraphicsRootConstantBufferView(0, eye.frameCb.GetGPUVirtualAddress());
				commandList->SetPipelineState(hatchGeometryPso.Get());
				commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				for (size_t c = 0; c < characterGeometry.size(); ++c) {
					float params[4] = { characters[c].worldOffset.x, characters[c].worldOffset.y, characters[c].worldOffset.z, (float)c };
					commandList->SetGraphicsRoot32BitConstants(1, 4, params, 0);
					characterGeometry[c]->Draw(commandList.Get());
				}

				D3D12_RESOURCE_BARRIER toSrv[2] = {
					CD3DX12_RESOURCE_BARRIER::Transition(eye.luminance.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, ReadState),
					CD3DX12_RESOURCE_BARRIER::Transition(eye.directionMap.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, ReadState),
				};
				commandList->ResourceBarrier(2, toSrv);
			}

			// -- Pass B: seed generation + luminance-driven filtering --
			{
				eye.seedCounterBuffer->Transition(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, commandList.Get());
				eye.seedBuffer->Transition(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, commandList.Get());

				commandList->SetComputeRootSignature(clearSeedCounterRootSig.Get());
				commandList->SetComputeRootUnorderedAccessView(0, eye.seedCounterBuffer->Get()->GetGPUVirtualAddress());
				commandList->SetPipelineState(clearSeedCounterPso.Get());
				commandList->Dispatch(1, 1, 1);
				commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(eye.seedCounterBuffer->Get()));

				commandList->SetComputeRootSignature(seedGenRootSig.Get());
				commandList->SetComputeRootConstantBufferView(0, eye.seedGenCb.GetGPUVirtualAddress());
				commandList->SetComputeRootDescriptorTable(1, eye.srvTableLumDir);
				commandList->SetComputeRootUnorderedAccessView(2, eye.seedBuffer->Get()->GetGPUVirtualAddress());
				commandList->SetComputeRootUnorderedAccessView(3, eye.seedCounterBuffer->Get()->GetGPUVirtualAddress());
				commandList->SetPipelineState(seedGenPso.Get());

				UINT cellSize = (UINT)std::max(1, seedCellSizePx);
				UINT cellsX = ((UINT)viewPort.Width + cellSize - 1) / cellSize;
				UINT cellsY = ((UINT)viewPort.Height + cellSize - 1) / cellSize;
				UINT groupsX = (cellsX + 7) / 8;
				UINT groupsY = (cellsY + 7) / 8;
				commandList->Dispatch(groupsX, groupsY, 1);

				eye.seedBuffer->Transition(ReadState, commandList.Get());
				eye.seedCounterBuffer->Transition(ReadState, commandList.Get());
			}

			// -- Pass C: stroke extrusion + draw --
			{
				D3D12_RESOURCE_BARRIER toRt = CD3DX12_RESOURCE_BARRIER::Transition(eye.strokeMask.Get(), ReadState, D3D12_RESOURCE_STATE_RENDER_TARGET);
				commandList->ResourceBarrier(1, &toRt);

				commandList->OMSetRenderTargets(1, &eye.rtvStrokeMask, FALSE, nullptr);
				commandList->RSSetViewports(1, &viewPort);
				commandList->RSSetScissorRects(1, &scissorRect);
				const float zero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				commandList->ClearRenderTargetView(eye.rtvStrokeMask, zero, 0, nullptr);

				commandList->SetGraphicsRootSignature(strokeRootSig.Get());
				commandList->SetGraphicsRootConstantBufferView(0, eye.strokeFrameCb.GetGPUVirtualAddress());
				commandList->SetGraphicsRootShaderResourceView(1, eye.seedBuffer->Get()->GetGPUVirtualAddress());
				commandList->SetGraphicsRootShaderResourceView(2, eye.seedCounterBuffer->Get()->GetGPUVirtualAddress());
				commandList->SetPipelineState(strokePso.Get());
				commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				commandList->DrawInstanced(6 * StrokeSegments, MaxSeeds, 0, 0);

				D3D12_RESOURCE_BARRIER toSrv = CD3DX12_RESOURCE_BARRIER::Transition(eye.strokeMask.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, ReadState);
				commandList->ResourceBarrier(1, &toSrv);
			}
		}

		// -- Pass A2: cross-projected hatching (Cross-Projected mode only) --
		// Must run AFTER both eyes' Pass C above, since eye e's pass here
		// reads the OTHER eye's now-finished strokeMask. Depth-testing this
		// pass against THIS eye's own (already-written, untouched since
		// Pass A) depth buffer, while simultaneously exposing the OTHER
		// eye's depth as an SRV, means only the OTHER eye's depth ever needs
		// a state transition -- see crossProjectPS.hlsl.
		if (renderMode == RenderMode::CrossProjected) {
			for (int e = 0; e < 2; ++e) {
				EyeResources& eye = eyes[e];
				EyeResources& other = eyes[1 - e];

				commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(other.eyeDepth.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, ReadState));

				D3D12_RESOURCE_BARRIER toRt = CD3DX12_RESOURCE_BARRIER::Transition(eye.crossMask.Get(), ReadState, D3D12_RESOURCE_STATE_RENDER_TARGET);
				commandList->ResourceBarrier(1, &toRt);

				commandList->OMSetRenderTargets(1, &eye.rtvCrossMask, FALSE, &eye.dsv);
				commandList->RSSetViewports(1, &viewPort);
				commandList->RSSetScissorRects(1, &scissorRect);
				const float zero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				commandList->ClearRenderTargetView(eye.rtvCrossMask, zero, 0, nullptr);
				// eye.dsv is NOT cleared here -- reusing Pass A's already-
				// written depth as a read-only test reference.

				commandList->SetGraphicsRootSignature(crossProjectRootSig.Get());
				commandList->SetGraphicsRootConstantBufferView(0, eye.crossProjectCb.GetGPUVirtualAddress());
				commandList->SetGraphicsRootDescriptorTable(2, crossInputTableGpu[e]);
				commandList->SetPipelineState(crossProjectPso.Get());
				commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				for (size_t c = 0; c < characterGeometry.size(); ++c) {
					float params[4] = { characters[c].worldOffset.x, characters[c].worldOffset.y, characters[c].worldOffset.z, 0.0f };
					commandList->SetGraphicsRoot32BitConstants(1, 4, params, 0);
					characterGeometry[c]->Draw(commandList.Get());
				}

				D3D12_RESOURCE_BARRIER toSrv = CD3DX12_RESOURCE_BARRIER::Transition(eye.crossMask.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, ReadState);
				commandList->ResourceBarrier(1, &toSrv);

				commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(other.eyeDepth.Get(), ReadState, D3D12_RESOURCE_STATE_DEPTH_WRITE));
			}
		}

		// -- Pass D: anaglyph composite --
		{
			commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
				renderTargets[swapChainBackBufferIndex].Get(),
				D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

			CD3DX12_CPU_DESCRIPTOR_HANDLE rHandle(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
				swapChainBackBufferIndex, rtvDescriptorHandleIncrementSize);

			commandList->OMSetRenderTargets(1, &rHandle, FALSE, nullptr);
			commandList->RSSetViewports(1, &viewPort);
			commandList->RSSetScissorRects(1, &scissorRect);
			const float bg[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
			commandList->ClearRenderTargetView(rHandle, bg, 0, nullptr);

			commandList->SetGraphicsRootSignature(compositeRootSig.Get());
			commandList->SetGraphicsRootConstantBufferView(0, compositeCb.GetGPUVirtualAddress());
			commandList->SetGraphicsRootDescriptorTable(1, compositeTableGpu);
			commandList->SetPipelineState(compositePso.Get());
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			commandList->DrawInstanced(3, 1, 0, 0);
		}

		BuildImGui();

		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			renderTargets[swapChainBackBufferIndex].Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

		DX_API("close graphics command list") commandList->Close();
	}

	virtual void Update(float dt, float T) override {
		using namespace Egg::Math;

		if (headCamera) headCamera->Animate(dt);

		float az = lightAzimuthDeg * (3.14159265f / 180.0f);
		float el = lightElevationDeg * (3.14159265f / 180.0f);
		float3 lightDir = float3(std::cos(el) * std::cos(az), std::sin(el), std::cos(el) * std::sin(az)).Normalize();

		if (headCamera) {
			// Both eyes' viewProj computed up front -- eye e's crossProjectCb
			// needs the OTHER eye's matrix too, which isn't available yet if
			// filled in a single per-eye pass in iteration order.
			float4x4 eyeViewProj[2];
			float3 eyePos[2];
			for (int e = 0; e < 2; ++e) {
				bool right = (e == 1);
				float4x4 view = Hatch::ComputeEyeView(headCamera, eyeSeparation, right);
				float4x4 proj = Hatch::ComputeEyeProj(fov, aspectRatio, nearPlane, farPlane, eyeSeparation, convergenceDistance, right);
				eyeViewProj[e] = view * proj;
				eyePos[e] = headCamera->GetEyePosition() + Hatch::HeadRight(headCamera) * (right ? 0.5f : -0.5f) * eyeSeparation;
			}

			for (int e = 0; e < 2; ++e) {
				int other = 1 - e;

				eyes[e].frameCb.data.viewProjTransform = eyeViewProj[e];
				eyes[e].frameCb.data.lightDirWorld = float4(lightDir, 0.0f);
				eyes[e].frameCb.data.lightColor = float4(1.0f * lightIntensity, 1.0f * lightIntensity, 1.0f * lightIntensity, ambient);
				eyes[e].frameCb.data.viewportParams = float4(viewPort.Width, viewPort.Height, 0.0f, 0.0f);
				eyes[e].frameCb.data.cameraPosWorld = float4(eyePos[e], 0.0f);
				eyes[e].frameCb.data.brdfParams = float4(kd, ks, specExponent, 0.0f);
				eyes[e].frameCb.Upload();

				eyes[e].crossProjectCb.data.ownViewProjTransform = eyeViewProj[e];
				eyes[e].crossProjectCb.data.otherViewProjTransform = eyeViewProj[other];
				eyes[e].crossProjectCb.data.viewportParams = float4(viewPort.Width, viewPort.Height, CrossProjectDepthEpsilon, 0.0f);
				eyes[e].crossProjectCb.Upload();

				eyes[e].seedGenCb.data.eyeSeed = eyes[e].hashSeed;
				eyes[e].seedGenCb.data.cellSizePx = (float)seedCellSizePx;
				eyes[e].seedGenCb.data.densityGamma = densityGamma;
				eyes[e].seedGenCb.data.seedDensityScale = seedDensityScale;
				eyes[e].seedGenCb.data.maxSeeds = (uint)maxSeedsSetting;
				eyes[e].seedGenCb.data.viewportWidthPx = (uint)viewPort.Width;
				eyes[e].seedGenCb.data.viewportHeightPx = (uint)viewPort.Height;
				eyes[e].seedGenCb.data._pad0 = 0;
				eyes[e].seedGenCb.Upload();

				eyes[e].strokeFrameCb.data.viewportParams = float4(viewPort.Width, viewPort.Height, 0.0f, 0.0f);
				eyes[e].strokeFrameCb.data.strokeParams = float4(strokeLengthPx, strokeWidthPx, (float)maxSeedsSetting, curvatureBendScale);
				eyes[e].strokeFrameCb.Upload();
			}
		}

		compositeCb.data.paperColor = float4(paperColorRgb[0], paperColorRgb[1], paperColorRgb[2], 1.0f);
		compositeCb.data.inkTint = float4(leftInkStrength, rightInkStrength, showLuminanceMap ? 1.0f : 0.0f, 0.0f);
		compositeCb.data.modeParams = float4(
			(renderMode == RenderMode::CrossProjected) ? 1.0f : 0.0f,
			showUnprojectedHatching ? 1.0f : 0.0f,
			showCrossProjectedHatching ? 1.0f : 0.0f,
			0.0f);
		compositeCb.Upload();
	}

	virtual void ProcessMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override {
		if (headCamera) headCamera->ProcessMessage(hWnd, uMsg, wParam, lParam);

		if (uMsg == WM_KEYDOWN && ImGui::GetCurrentContext() && !ImGui::GetIO().WantCaptureKeyboard) {
			if (wParam == 'R') needsRebuildCharacters = true;
		}
	}

	virtual void ReleaseSwapChainResources() override {
		for (int e = 0; e < 2; ++e) {
			eyes[e].sceneColor.Reset();
			eyes[e].luminance.Reset();
			eyes[e].directionMap.Reset();
			eyes[e].strokeMask.Reset();
			eyes[e].crossMask.Reset();
			eyes[e].eyeDepth.Reset();
		}
		eyeRtvHeap.Reset();
		eyeDsvHeap.Reset();
		mainAlloc = nullptr;
		Egg::SimpleApp::ReleaseSwapChainResources();
	}

	virtual void ReleaseResources() override {
		compositeCb.ReleaseResources();
		for (int e = 0; e < 2; ++e) {
			eyes[e].frameCb.ReleaseResources();
			eyes[e].seedGenCb.ReleaseResources();
			eyes[e].strokeFrameCb.ReleaseResources();
			eyes[e].crossProjectCb.ReleaseResources();
			eyes[e].seedBuffer = nullptr;
			eyes[e].seedCounterBuffer = nullptr;
		}
		imguiSrvHeap.Reset();
		hatchGeometryRootSig.Reset(); hatchGeometryPso.Reset();
		clearSeedCounterRootSig.Reset(); clearSeedCounterPso.Reset();
		seedGenRootSig.Reset(); seedGenPso.Reset();
		strokeRootSig.Reset(); strokePso.Reset();
		compositeRootSig.Reset(); compositePso.Reset();
		crossProjectRootSig.Reset(); crossProjectPso.Reset();
		characterGeometry.clear();
		Egg::SimpleApp::ReleaseResources();
	}
};
