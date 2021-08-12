#include "Egg/Common.h"
#include <d3d11_2.h>

#include "Game.h"
#include "Egg11/ThrowOnFail.h"
#include "Egg11/Mesh/Importer.h"
#include "Egg11/Mesh/Importer.h"
#include <DirectXTex/DirectXTex.h>
#include "App/UtfConverter.h"
#include "DDSTextureLoader.h"
#include <assimp/importer.hpp>      
#include <assimp/scene.h>       
#include <assimp/postProcess.h> 
#include "DualQuaternion.h"

#include <iostream> 

using namespace Egg11::Math;

const unsigned int defaultParticleCount = 1024 * 2;
//const unsigned int defaultParticleCount = 256;
//const unsigned int controlParticleCount = 1024 * 8;
const unsigned int controlParticleCount = 4096;
const unsigned int linkbufferSizePerPixel = 256;
const unsigned int sbufferSizePerPixel = 512;
const unsigned int hashCount = 13;
const unsigned int PBDGrideSize = 16;
//const Egg11::Math::float3 PBDGrideTrans = Egg11::Math::float3(0.0, 0.5, 0.0);
//const Egg11::Math::float4x4 PBDGrideTrans = Egg11::Math::float4x4::translation(float3(0.0, 0.5, 0.0)) * Egg11::Math::float4x4::rotation(float3(1.0, 1.0, 1.0).normalize (), 3.14/2);



Game::Game(Microsoft::WRL::ComPtr<ID3D11Device> device) : Egg11::App(device)
{
}

Game::~Game(void)
{
}

HRESULT Game::createResources()
{
	CreateCommon();
	CreateParticles();
	CreateControlMesh();
	CreateControlParticles();
	CreateBillboard();
	CreateBillboardForControlParticles();
	CreatePrefixSum();
	CreateEnviroment();
	CreateMetaball();
	CreateAnimation();
	CreateDebug();


	//controlParams[5] = 1.0;
	controlParams[7] = 0.0;

	return S_OK;
}

void Game::CreateCommon()
{
	inputBinder = Egg11::Mesh::InputBinder::create(device);

	firstPersonCam = Egg11::Cam::FirstPerson::create();

	firstPersonCam->setAspect(windowWidth / windowHeight);

	billboardsLoadAlgorithm = SBuffer;
	renderMode = Gradient;
	flowControl = RealisticFlow;
	controlParticlePlacement = PBD;
	metalShading = Gold;
	shading = PhongShading;
	metaballFunction = Wyvill;
	waterShading = SimpleWater;

	billboardsLoadAlgorithm = Normal;
	renderMode = ControlParticles;

	drawFlatControlMesh = false;
	animtedIsActive = true;
	adapticeControlPressureIsActive = true;
	controlParticleAnimtaionIsActive = false;

	//radius = 1.0;
	radius = 1.1;
	metaBallMinToHit = 0.9;
	binaryStepCount = 2;
	maxRecursion = 2;
	marchCount = 25;

	debugType = 0;
	testCount = 0;
	testCount2 = 0;

	// modelViewProjCB
	D3D11_BUFFER_DESC modelViewProjCBDesc;
	modelViewProjCBDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	modelViewProjCBDesc.ByteWidth = sizeof(Egg11::Math::float4x4) * 4;
	modelViewProjCBDesc.CPUAccessFlags = 0;
	modelViewProjCBDesc.MiscFlags = 0;
	modelViewProjCBDesc.StructureByteStride = 0;
	modelViewProjCBDesc.Usage = D3D11_USAGE_DEFAULT;
	Egg11::ThrowOnFail("Failed to create billboardGSTransCB.", __FILE__, __LINE__) ^
		device->CreateBuffer(&modelViewProjCBDesc, nullptr, modelViewProjCB.GetAddressOf());

	// eyePosCB
	D3D11_BUFFER_DESC eyePosCBDesc;
	eyePosCBDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	eyePosCBDesc.ByteWidth = sizeof(Egg11::Math::float4x4) * 4;
	eyePosCBDesc.CPUAccessFlags = 0;
	eyePosCBDesc.MiscFlags = 0;
	eyePosCBDesc.StructureByteStride = 0;
	eyePosCBDesc.Usage = D3D11_USAGE_DEFAULT;
	Egg11::ThrowOnFail("Failed to create metaballPerFrameConstantBuffer.", __FILE__, __LINE__) ^
		device->CreateBuffer(&eyePosCBDesc, nullptr, eyePosCB.GetAddressOf());

	// shadingCB
	D3D11_BUFFER_DESC shadingCBDesc;
	shadingCBDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	shadingCBDesc.ByteWidth = sizeof(Egg11::Math::float4) * 8;
	shadingCBDesc.CPUAccessFlags = 0;
	shadingCBDesc.MiscFlags = 0;
	shadingCBDesc.StructureByteStride = 0;
	shadingCBDesc.Usage = D3D11_USAGE_DEFAULT;
	Egg11::ThrowOnFail("Failed to create shadingCB.", __FILE__, __LINE__) ^
		device->CreateBuffer(&shadingCBDesc, nullptr, shadingCB.GetAddressOf());

	// shadingTypeCB
	D3D11_BUFFER_DESC shadingTypeCBDesc;
	shadingTypeCBDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	shadingTypeCBDesc.ByteWidth = sizeof(float) * 8;
	shadingTypeCBDesc.CPUAccessFlags = 0;
	shadingTypeCBDesc.MiscFlags = 0;
	shadingTypeCBDesc.StructureByteStride = 0;
	shadingTypeCBDesc.Usage = D3D11_USAGE_DEFAULT;
	Egg11::ThrowOnFail("Failed to create shadingTypeCB.", __FILE__, __LINE__) ^
		device->CreateBuffer(&shadingTypeCBDesc, nullptr, shadingTypeCB.GetAddressOf());

	// metaballFunctionCB
	D3D11_BUFFER_DESC metaballFunctionCBDesc;
	metaballFunctionCBDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	metaballFunctionCBDesc.ByteWidth = sizeof(float) * 4;
	metaballFunctionCBDesc.CPUAccessFlags = 0;
	metaballFunctionCBDesc.MiscFlags = 0;
	metaballFunctionCBDesc.StructureByteStride = 0;
	metaballFunctionCBDesc.Usage = D3D11_USAGE_DEFAULT;
	Egg11::ThrowOnFail("Failed to create metaballFunctionCB.", __FILE__, __LINE__) ^
		device->CreateBuffer(&metaballFunctionCBDesc, nullptr, metaballFunctionCB.GetAddressOf());
}

void Game::CreateParticles()
{
	std::vector<Particle> particles;

	// Create Particles
	for (int i = 0; i < defaultParticleCount; i++)
		particles.push_back(Particle());

	{
		// Data Buffer
		D3D11_BUFFER_DESC particleBufferDesc;
		particleBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		particleBufferDesc.CPUAccessFlags = 0;
		particleBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		particleBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		particleBufferDesc.StructureByteStride = sizeof(Particle);
		particleBufferDesc.ByteWidth = defaultParticleCount * sizeof(Particle);

		D3D11_SUBRESOURCE_DATA initialParticleData;
		initialParticleData.pSysMem = &particles.at(0);

		Egg11::ThrowOnFail("Could not create particleDataBuffer.", __FILE__, __LINE__) ^
			device->CreateBuffer(&particleBufferDesc, &initialParticleData, particleDataBuffer.GetAddressOf());


		// Shader Resource View
		D3D11_SHADER_RESOURCE_VIEW_DESC particleSRVDesc;
		particleSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		particleSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
		particleSRVDesc.Buffer.FirstElement = 0;
		particleSRVDesc.Buffer.NumElements = defaultParticleCount;

		Egg11::ThrowOnFail("Could not create metaballVSParticleSRV.", __FILE__, __LINE__) ^
			device->CreateShaderResourceView(particleDataBuffer.Get(), &particleSRVDesc, &particleSRV);


		// Unordered Access View
		D3D11_UNORDERED_ACCESS_VIEW_DESC particleUAVDesc;
		particleUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		particleUAVDesc.Format = DXGI_FORMAT_UNKNOWN;
		particleUAVDesc.Buffer.FirstElement = 0;
		particleUAVDesc.Buffer.NumElements = defaultParticleCount;
		particleUAVDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_COUNTER; // WHY????

		Egg11::ThrowOnFail("Could not create animationUAV.", __FILE__, __LINE__) ^
			device->CreateUnorderedAccessView(particleDataBuffer.Get(), &particleUAVDesc, &particleUAV);
	}


	/// Hashtables
	{
		// Data Buffer
		D3D11_BUFFER_DESC particleBufferDesc;
		particleBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		particleBufferDesc.CPUAccessFlags = 0;
		particleBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		particleBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
		particleBufferDesc.StructureByteStride = sizeof(uint32_t);
		particleBufferDesc.ByteWidth = (defaultParticleCount + 1) * sizeof(uint32_t);

		Egg11::ThrowOnFail("Could not create particleDataBuffer.", __FILE__, __LINE__) ^
			device->CreateBuffer(&particleBufferDesc, NULL, clistDataBuffer.GetAddressOf());


		// Shader Resource View
		D3D11_SHADER_RESOURCE_VIEW_DESC particleSRVDesc;
		particleSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		particleSRVDesc.Format = DXGI_FORMAT_R32_UINT;
		particleSRVDesc.Buffer.FirstElement = 0;
		particleSRVDesc.Buffer.NumElements = defaultParticleCount + 1;

		Egg11::ThrowOnFail("Could not create metaballVSParticleSRV.", __FILE__, __LINE__) ^
			device->CreateShaderResourceView(clistDataBuffer.Get(), &particleSRVDesc, &clistSRV);


		// Unordered Access View
		D3D11_UNORDERED_ACCESS_VIEW_DESC particleUAVDesc;
		particleUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		particleUAVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		particleUAVDesc.Buffer.FirstElement = 0;
		particleUAVDesc.Buffer.NumElements = defaultParticleCount + 1;
		particleUAVDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;

		Egg11::ThrowOnFail("Could not create animationUAV.", __FILE__, __LINE__) ^
			device->CreateUnorderedAccessView(clistDataBuffer.Get(), &particleUAVDesc, &clistUAV);
	}
	{
		// Data Buffer
		D3D11_BUFFER_DESC particleBufferDesc;
		particleBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		particleBufferDesc.CPUAccessFlags = 0;
		particleBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		particleBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
		particleBufferDesc.StructureByteStride = sizeof(uint32_t);
		particleBufferDesc.ByteWidth = (defaultParticleCount + 1) * sizeof(uint32_t);

		Egg11::ThrowOnFail("Could not create particleDataBuffer.", __FILE__, __LINE__) ^
			device->CreateBuffer(&particleBufferDesc, NULL, clistLengthDataBuffer.GetAddressOf());


		// Shader Resource View
		D3D11_SHADER_RESOURCE_VIEW_DESC particleSRVDesc;
		particleSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		particleSRVDesc.Format = DXGI_FORMAT_R32_UINT;
		particleSRVDesc.Buffer.FirstElement = 0;
		particleSRVDesc.Buffer.NumElements = defaultParticleCount + 1;

		Egg11::ThrowOnFail("Could not create metaballVSParticleSRV.", __FILE__, __LINE__) ^
			device->CreateShaderResourceView(clistLengthDataBuffer.Get(), &particleSRVDesc, &clistLengthSRV);


		// Unordered Access View
		D3D11_UNORDERED_ACCESS_VIEW_DESC particleUAVDesc;
		particleUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		particleUAVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		particleUAVDesc.Buffer.FirstElement = 0;
		particleUAVDesc.Buffer.NumElements = defaultParticleCount + 1;
		particleUAVDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;

		Egg11::ThrowOnFail("Could not create animationUAV.", __FILE__, __LINE__) ^
			device->CreateUnorderedAccessView(clistLengthDataBuffer.Get(), &particleUAVDesc, &clistLengthUAV);
	}
	{
		// Data Buffer
		D3D11_BUFFER_DESC particleBufferDesc;
		particleBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		particleBufferDesc.CPUAccessFlags = 0;
		particleBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		particleBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
		particleBufferDesc.StructureByteStride = sizeof(uint32_t);
		particleBufferDesc.ByteWidth = (defaultParticleCount + 1) * sizeof(uint32_t);

		Egg11::ThrowOnFail("Could not create particleDataBuffer.", __FILE__, __LINE__) ^
			device->CreateBuffer(&particleBufferDesc, NULL, clistBeginDataBuffer.GetAddressOf());


		// Shader Resource View
		D3D11_SHADER_RESOURCE_VIEW_DESC particleSRVDesc;
		particleSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		particleSRVDesc.Format = DXGI_FORMAT_R32_UINT;
		particleSRVDesc.Buffer.FirstElement = 0;
		particleSRVDesc.Buffer.NumElements = defaultParticleCount + 1;

		Egg11::ThrowOnFail("Could not create metaballVSParticleSRV.", __FILE__, __LINE__) ^
			device->CreateShaderResourceView(clistBeginDataBuffer.Get(), &particleSRVDesc, &clistBeginSRV);


		// Unordered Access View
		D3D11_UNORDERED_ACCESS_VIEW_DESC particleUAVDesc;
		particleUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		particleUAVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		particleUAVDesc.Buffer.FirstElement = 0;
		particleUAVDesc.Buffer.NumElements = defaultParticleCount + 1;
		particleUAVDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;

		Egg11::ThrowOnFail("Could not create animationUAV.", __FILE__, __LINE__) ^
			device->CreateUnorderedAccessView(clistBeginDataBuffer.Get(), &particleUAVDesc, &clistBeginUAV);
	}

	{
		// Data Buffer
		D3D11_BUFFER_DESC particleBufferDesc;
		particleBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		particleBufferDesc.CPUAccessFlags = 0;
		particleBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		particleBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
		particleBufferDesc.StructureByteStride = sizeof(uint32_t);
		particleBufferDesc.ByteWidth = 1 * sizeof(uint32_t);

		Egg11::ThrowOnFail("Could not create particleDataBuffer.", __FILE__, __LINE__) ^
			device->CreateBuffer(&particleBufferDesc, NULL, clistCellCountDataBuffer.GetAddressOf());


		// Shader Resource View
		D3D11_SHADER_RESOURCE_VIEW_DESC particleSRVDesc;
		particleSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		particleSRVDesc.Format = DXGI_FORMAT_R32_UINT;
		particleSRVDesc.Buffer.FirstElement = 0;
		particleSRVDesc.Buffer.NumElements = 1;

		Egg11::ThrowOnFail("Could not create metaballVSParticleSRV.", __FILE__, __LINE__) ^
			device->CreateShaderResourceView(clistCellCountDataBuffer.Get(), &particleSRVDesc, &clistCellCountSRV);


		// Unordered Access View
		D3D11_UNORDERED_ACCESS_VIEW_DESC particleUAVDesc;
		particleUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		particleUAVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		particleUAVDesc.Buffer.FirstElement = 0;
		particleUAVDesc.Buffer.NumElements = 1;
		particleUAVDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;

		Egg11::ThrowOnFail("Could not create animationUAV.", __FILE__, __LINE__) ^
			device->CreateUnorderedAccessView(clistCellCountDataBuffer.Get(), &particleUAVDesc, &clistCellCountUAV);
	}

	{
		// Data Buffer
		D3D11_BUFFER_DESC particleBufferDesc;
		particleBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		particleBufferDesc.CPUAccessFlags = 0;
		particleBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		particleBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
		particleBufferDesc.StructureByteStride = sizeof(uint32_t);
		particleBufferDesc.ByteWidth = (defaultParticleCount) * sizeof(uint32_t);

		Egg11::ThrowOnFail("Could not create particleDataBuffer.", __FILE__, __LINE__) ^
			device->CreateBuffer(&particleBufferDesc, NULL, hlistDataBuffer.GetAddressOf());


		// Shader Resource View
		D3D11_SHADER_RESOURCE_VIEW_DESC particleSRVDesc;
		particleSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		particleSRVDesc.Format = DXGI_FORMAT_R32_UINT;
		particleSRVDesc.Buffer.FirstElement = 0;
		particleSRVDesc.Buffer.NumElements = defaultParticleCount;

		Egg11::ThrowOnFail("Could not create metaballVSParticleSRV.", __FILE__, __LINE__) ^
			device->CreateShaderResourceView(hlistDataBuffer.Get(), &particleSRVDesc, &hlistSRV);


		// Unordered Access View
		D3D11_UNORDERED_ACCESS_VIEW_DESC particleUAVDesc;
		particleUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		particleUAVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		particleUAVDesc.Buffer.FirstElement = 0;
		particleUAVDesc.Buffer.NumElements = defaultParticleCount;
		particleUAVDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;

		Egg11::ThrowOnFail("Could not create animationUAV.", __FILE__, __LINE__) ^
			device->CreateUnorderedAccessView(hlistDataBuffer.Get(), &particleUAVDesc, &hlistUAV);
	}
	{
		// Data Buffer
		D3D11_BUFFER_DESC particleBufferDesc;
		particleBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		particleBufferDesc.CPUAccessFlags = 0;
		particleBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		particleBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
		particleBufferDesc.StructureByteStride = sizeof(uint32_t);
		particleBufferDesc.ByteWidth = (hashCount) * sizeof(uint32_t);

		Egg11::ThrowOnFail("Could not create particleDataBuffer.", __FILE__, __LINE__) ^
			device->CreateBuffer(&particleBufferDesc, NULL, hlistLengthDataBuffer.GetAddressOf());


		// Shader Resource View
		D3D11_SHADER_RESOURCE_VIEW_DESC particleSRVDesc;
		particleSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		particleSRVDesc.Format = DXGI_FORMAT_R32_UINT;
		particleSRVDesc.Buffer.FirstElement = 0;
		particleSRVDesc.Buffer.NumElements = hashCount;

		Egg11::ThrowOnFail("Could not create metaballVSParticleSRV.", __FILE__, __LINE__) ^
			device->CreateShaderResourceView(hlistLengthDataBuffer.Get(), &particleSRVDesc, &hlistLengthSRV);


		// Unordered Access View
		D3D11_UNORDERED_ACCESS_VIEW_DESC particleUAVDesc;
		particleUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		particleUAVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		particleUAVDesc.Buffer.FirstElement = 0;
		particleUAVDesc.Buffer.NumElements = hashCount;
		particleUAVDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;

		Egg11::ThrowOnFail("Could not create animationUAV.", __FILE__, __LINE__) ^
			device->CreateUnorderedAccessView(hlistLengthDataBuffer.Get(), &particleUAVDesc, &hlistLengthUAV);
	}
	{
		// Data Buffer
		D3D11_BUFFER_DESC particleBufferDesc;
		particleBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		particleBufferDesc.CPUAccessFlags = 0;
		particleBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		particleBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
		particleBufferDesc.StructureByteStride = sizeof(uint32_t);
		particleBufferDesc.ByteWidth = (hashCount) * sizeof(uint32_t);

		Egg11::ThrowOnFail("Could not create particleDataBuffer.", __FILE__, __LINE__) ^
			device->CreateBuffer(&particleBufferDesc, NULL, hlistBeginDataBuffer.GetAddressOf());


		// Shader Resource View
		D3D11_SHADER_RESOURCE_VIEW_DESC particleSRVDesc;
		particleSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		particleSRVDesc.Format = DXGI_FORMAT_R32_UINT;
		particleSRVDesc.Buffer.FirstElement = 0;
		particleSRVDesc.Buffer.NumElements = hashCount;

		Egg11::ThrowOnFail("Could not create metaballVSParticleSRV.", __FILE__, __LINE__) ^
			device->CreateShaderResourceView(hlistBeginDataBuffer.Get(), &particleSRVDesc, &hlistBeginSRV);


		// Unordered Access View
		D3D11_UNORDERED_ACCESS_VIEW_DESC particleUAVDesc;
		particleUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		particleUAVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		particleUAVDesc.Buffer.FirstElement = 0;
		particleUAVDesc.Buffer.NumElements = hashCount;
		particleUAVDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;

		Egg11::ThrowOnFail("Could not create animationUAV.", __FILE__, __LINE__) ^
			device->CreateUnorderedAccessView(hlistBeginDataBuffer.Get(), &particleUAVDesc, &hlistBeginUAV);
	}

	{
		using namespace Microsoft::WRL;

		ComPtr<ID3DBlob> clistShaderInitByteCode = loadShaderCode("csCListInit.cso");
		clistShaderInit = Egg11::Mesh::Shader::create("csCListInit.cso", device, clistShaderInitByteCode);

		ComPtr<ID3DBlob> clistCompactByteCode = loadShaderCode("csCListCompact.cso");
		clistShaderCompact = Egg11::Mesh::Shader::create("csCListCompact.cso", device, clistCompactByteCode);

		ComPtr<ID3DBlob> clistLengthByteCode = loadShaderCode("csCListLength.cso");
		clistShaderLength = Egg11::Mesh::Shader::create("csCListLength.cso", device, clistLengthByteCode);

		ComPtr<ID3DBlob> clistShaderSortEvenByteCode = loadShaderCode("csCListSortEven.cso");
		clistShaderSortEven = Egg11::Mesh::Shader::create("csCListSortEven.cso", device, clistShaderSortEvenByteCode);

		ComPtr<ID3DBlob> clistShaderSortOddByteCode = loadShaderCode("csCListSortOdd.cso");
		clistShaderSortOdd = Egg11::Mesh::Shader::create("csCListSortOdd.cso", device, clistShaderSortOddByteCode);

		ComPtr<ID3DBlob> hlistShaderInitByteCode = loadShaderCode("csHListInit.cso");
		hlistShaderInit = Egg11::Mesh::Shader::create("csHListInit.cso", device, hlistShaderInitByteCode);

		ComPtr<ID3DBlob> hlistShaderBeginByteCode = loadShaderCode("csHListBegin.cso");
		hlistShaderBegin = Egg11::Mesh::Shader::create("csHListBegin.cso", device, hlistShaderBeginByteCode);

		ComPtr<ID3DBlob> hlistLengthByteCode = loadShaderCode("csHListLength.cso");
		hlistShaderLength = Egg11::Mesh::Shader::create("csHListLength.cso", device, hlistLengthByteCode);
	}
}

void Game::CreateControlParticles()
{
	using namespace Microsoft::WRL;

	std::vector<ControlParticle> controlParticles(controlParticleCount);

	Assimp::Importer importer;

	//const aiScene* assScene = importer.ReadFile(App::getSystemEnvironment().resolveMediaPath("deer.obj"), 0);
	//controlMeshScale = 0.0003;

	//const aiScene* assScene = importer.ReadFile(App::getSystemEnvironment().resolveMediaPath("DetailedDeer2.obj"), 0);
	//controlMeshScale = 0.0035;



	const aiScene* assScene = importer.ReadFile(App::getSystemEnvironment().resolveMediaPath("giraffe3.obj"), 0);
	controlMeshScale = 0.5;

	//const aiScene* assScene = importer.ReadFile(App::getSystemEnvironment().resolveMediaPath("castle_guard_03.dae"), 0);
	//controlMeshScale =animatedControlMeshScale;

	//const aiScene* assScene = importer.ReadFile(App::getSystemEnvironment().resolveMediaPath("kachujin.dae"), 0);
	//controlMeshScale = 0.0036;

	//const aiScene* assScene = importer.ReadFile(App::getSystemEnvironment().resolveMediaPath("xbot.dae"), 0);
	//controlMeshScale =animatedControlMeshScale;



	//const aiScene* assScene = importer.ReadFile(App::getSystemEnvironment().resolveMediaPath("Dragon2.obj"), 0);
	//controlMeshScale = 0.005;

	//const aiScene* assScene = importer.ReadFile(App::getSystemEnvironment().resolveMediaPath("giraffe.obj"), 0);
	//const aiScene* assScene = importer.ReadFile(App::getSystemEnvironment().resolveMediaPath("lion.obj"), 0);

	if (controlParticlePlacement == Vertex)
	{
		// Create Particles
		for (int i = 0; i < std::min(assScene->mMeshes[0]->mNumFaces / 3, controlParticleCount); i++)
		{
			ControlParticle cp;
			cp.position.x = assScene->mMeshes[0]->mVertices[i].x;
			cp.position.y = assScene->mMeshes[0]->mVertices[i].y;
			cp.position.z = assScene->mMeshes[0]->mVertices[i].z;
			cp.position *= 0.0002;
			cp.temp = 0.0f;
			controlParticles.push_back(cp);
		}
	}
	if (controlParticlePlacement == PBD) {
		for (int i = 0; i < PBDGrideSize; i++)
		{
			for (int j = 0; j < PBDGrideSize; j++)
			{
				for (int k = 0; k < PBDGrideSize; k++)
				{
					ControlParticle cp;
					memset(&cp, 0, sizeof (ControlParticle));
					//cp.position.x = k * 0.01;
					//cp.position.y = j * 0.01;
					//cp.position.z = i * 0.01;
					//cp.position += PBDGrideTrans;
					float4 defaultPos (k * 0.01, j * 0.01, i * 0.01, 1.0);
					//const Egg11::Math::float4x4 PBDGrideTrans = Egg11::Math::float4x4::identity;
					const Egg11::Math::float4x4 PBDGrideTrans = Egg11::Math::float4x4::rotation(float3(1.0, 1.0, 1.0).normalize(),  3.14 / 2.0) * Egg11::Math::float4x4::translation(float3(0.0, 0.5, 0.0));
					//const Egg11::Math::float4x4 PBDGrideTrans = Egg11::Math::float4x4::translation(float3(0.0, 0.5, 0.0));
					defaultPos = defaultPos * PBDGrideTrans;
					cp.position = defaultPos.xyz;


					cp.controlPressureRatio = 1.0;
					cp.temp = 0.0f;					
					//controlParticles.push_back(cp);
					controlParticles[i * PBDGrideSize * PBDGrideSize + j * PBDGrideSize + k] = (cp);
				}
			}
		}
	}
	//else 
	if (controlParticlePlacement == Render || controlParticlePlacement == Animated)
	{
		fillCam = Egg11::Cam::FirstPerson::create();
		fillCam->setView(float3(0.0, 0.5, -0.5), float3(0, 0, 1));

		// First round
		{
			Egg11::Mesh::Geometry::P geometry = Egg11::Mesh::Importer::fromAiMesh(device, assScene->mMeshes[0]);

			ComPtr<ID3DBlob> vertexShaderByteCode = loadShaderCode("vsTrafo.cso");
			Egg11::Mesh::Shader::P vertexShader = Egg11::Mesh::Shader::create("vsTrafo.cso", device, vertexShaderByteCode);

			//ComPtr<ID3DBlob> pixelShaderByteCode = loadShaderCode("psIdle.cso");
			//Egg11::Mesh::Shader::P pixelShader = Egg11::Mesh::Shader::create("psIdle.cso", device, pixelShaderByteCode);

			ComPtr<ID3DBlob> pixelShaderByteCode = loadShaderCode("psControlMeshA.cso");
			Egg11::Mesh::Shader::P pixelShader = Egg11::Mesh::Shader::create("psControlMeshA.cso", device, pixelShaderByteCode);

			Egg11::Mesh::Material::P material = Egg11::Mesh::Material::create();
			material->setShader(Egg11::Mesh::ShaderStageFlag::Vertex, vertexShader);
			material->setShader(Egg11::Mesh::ShaderStageFlag::Pixel, pixelShader);
			material->setCb("modelViewProjCB", modelViewProjCB, Egg11::Mesh::ShaderStageFlag::Vertex);
			material->setCb("modelViewProjCB", modelViewProjCB, Egg11::Mesh::ShaderStageFlag::Pixel);

			// Depth settings
			Microsoft::WRL::ComPtr<ID3D11DepthStencilState> DSState;
			D3D11_DEPTH_STENCIL_DESC dsDesc;

			// Depth test parameters
			dsDesc.DepthEnable = false;
			dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
			dsDesc.DepthFunc = D3D11_COMPARISON_LESS;

			// Stencil test parameters
			dsDesc.StencilEnable = false;
			dsDesc.StencilReadMask = 0xFF;
			dsDesc.StencilWriteMask = 0xFF;

			// Stencil operations if pixel is front-facing
			dsDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
			dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
			dsDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
			dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

			// Stencil operations if pixel is back-facing
			dsDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
			dsDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
			dsDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
			dsDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

			// Create depth stencil state
			device->CreateDepthStencilState(&dsDesc, DSState.GetAddressOf());
			material->depthStencilState = DSState;

			/// Raster settings
			Microsoft::WRL::ComPtr<ID3D11RasterizerState> RasterizerState;

			D3D11_RASTERIZER_DESC RasterizerDesc;
			RasterizerDesc.CullMode = D3D11_CULL_NONE;
			RasterizerDesc.FillMode = D3D11_FILL_SOLID;
			RasterizerDesc.FrontCounterClockwise = FALSE;
			RasterizerDesc.DepthBias = D3D11_DEFAULT_DEPTH_BIAS;
			RasterizerDesc.DepthBiasClamp = D3D11_DEFAULT_DEPTH_BIAS_CLAMP;
			RasterizerDesc.SlopeScaledDepthBias = D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
			RasterizerDesc.DepthClipEnable = TRUE;
			RasterizerDesc.ScissorEnable = FALSE;
			RasterizerDesc.MultisampleEnable = FALSE;
			RasterizerDesc.AntialiasedLineEnable = FALSE;

			device->CreateRasterizerState(&RasterizerDesc, RasterizerState.GetAddressOf());
			material->rasterizerState = RasterizerState;


			ComPtr<ID3D11InputLayout> inputLayout = inputBinder->getCompatibleInputLayout(vertexShaderByteCode, geometry);
			controlMesh = Egg11::Mesh::Shaded::create(geometry, material, inputLayout);

			//Debug
			{
				ComPtr<ID3DBlob> vertexShaderByteCodeDebug = loadShaderCode("vsTrafoNorm.cso");
				Egg11::Mesh::Shader::P vertexShaderDebug = Egg11::Mesh::Shader::create("vsTrafoNorm.cso", device, vertexShaderByteCodeDebug);

				ComPtr<ID3DBlob> pixelShaderByteCodeDebug = loadShaderCode("psFlat.cso");
				Egg11::Mesh::Shader::P pixelShaderDebug = Egg11::Mesh::Shader::create("psFlat.cso", device, pixelShaderByteCodeDebug);

				Egg11::Mesh::Material::P materialDebug = Egg11::Mesh::Material::create();
				materialDebug->setShader(Egg11::Mesh::ShaderStageFlag::Vertex, vertexShaderDebug);
				materialDebug->setShader(Egg11::Mesh::ShaderStageFlag::Pixel, pixelShaderDebug);
				materialDebug->setCb("modelViewProjCB", modelViewProjCB, Egg11::Mesh::ShaderStageFlag::Vertex);
				materialDebug->setCb("modelViewProjCB", modelViewProjCB, Egg11::Mesh::ShaderStageFlag::Pixel);

				ComPtr<ID3D11InputLayout> inputLayoutDebug = inputBinder->getCompatibleInputLayout(vertexShaderByteCodeDebug, geometry);
				controlMeshFlat = Egg11::Mesh::Shaded::create(geometry, materialDebug, inputLayoutDebug);
			}

		}


		// Second round
		{
			// Shaders
			Egg11::Mesh::Geometry::P fullQuadGeometry = Egg11::Mesh::Indexed::createQuad(device);

			ComPtr<ID3DBlob> vertexShaderByteCode = loadShaderCode("vsControlMeshFill.cso");
			Egg11::Mesh::Shader::P vertexShader = Egg11::Mesh::Shader::create("vsControlMeshFill.cso", device, vertexShaderByteCode);

			ComPtr<ID3DBlob> pixelShaderByteCode = loadShaderCode("psControlMeshFill.cso");
			Egg11::Mesh::Shader::P pixelShader = Egg11::Mesh::Shader::create("psControlMeshFill.cso", device, pixelShaderByteCode);

			Egg11::Mesh::Material::P material = Egg11::Mesh::Material::create();
			material->setShader(Egg11::Mesh::ShaderStageFlag::Vertex, vertexShader);
			material->setShader(Egg11::Mesh::ShaderStageFlag::Pixel, pixelShader);


			/// Depth settings
			Microsoft::WRL::ComPtr<ID3D11DepthStencilState> DSState;
			D3D11_DEPTH_STENCIL_DESC dsDesc;

			// Depth test parameters
			dsDesc.DepthEnable = false;
			dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
			dsDesc.DepthFunc = D3D11_COMPARISON_LESS;

			// Stencil test parameters
			dsDesc.StencilEnable = false;
			dsDesc.StencilReadMask = 0xFF;
			dsDesc.StencilWriteMask = 0xFF;

			// Stencil operations if pixel is front-facing
			dsDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
			dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
			dsDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
			dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

			// Stencil operations if pixel is back-facing
			dsDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
			dsDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
			dsDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
			dsDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

			// Create depth stencil state
			device->CreateDepthStencilState(&dsDesc, DSState.GetAddressOf());
			material->depthStencilState = DSState;


			ComPtr<ID3D11InputLayout> inputLayout = inputBinder->getCompatibleInputLayout(vertexShaderByteCode, fullQuadGeometry);
			controlMeshFill = Egg11::Mesh::Shaded::create(fullQuadGeometry, material, inputLayout);
		}

	}

	{
		// Data Buffer
		D3D11_BUFFER_DESC particleBufferDesc;
		particleBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		particleBufferDesc.CPUAccessFlags = 0;
		particleBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		particleBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		particleBufferDesc.StructureByteStride = sizeof(ControlParticle);
		particleBufferDesc.ByteWidth = controlParticleCount * sizeof(ControlParticle);

		D3D11_SUBRESOURCE_DATA initialParticleData;
		initialParticleData.pSysMem = &controlParticles.at(0);

		Egg11::ThrowOnFail("Could not create particleDataBuffer.", __FILE__, __LINE__) ^
			device->CreateBuffer(&particleBufferDesc, &initialParticleData, controlParticleDataBuffer.GetAddressOf());


		// Shader Resource View
		D3D11_SHADER_RESOURCE_VIEW_DESC particleSRVDesc;
		particleSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		particleSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
		particleSRVDesc.Buffer.FirstElement = 0;
		particleSRVDesc.Buffer.NumElements = controlParticles.size();

		Egg11::ThrowOnFail("Could not create metaballVSParticleSRV.", __FILE__, __LINE__) ^
			device->CreateShaderResourceView(controlParticleDataBuffer.Get(), &particleSRVDesc, &controlParticleSRV);


		// Unordered Access View
		D3D11_UNORDERED_ACCESS_VIEW_DESC particleUAVDesc;
		particleUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		particleUAVDesc.Format = DXGI_FORMAT_UNKNOWN;
		particleUAVDesc.Buffer.FirstElement = 0;
		particleUAVDesc.Buffer.NumElements = controlParticles.size();
		particleUAVDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_COUNTER; // WHY????

		Egg11::ThrowOnFail("Could not create animationUAV.", __FILE__, __LINE__) ^
			device->CreateUnorderedAccessView(controlParticleDataBuffer.Get(), &particleUAVDesc, &controlParticleUAV);

	}

	if (controlParticlePlacement = PBD) {
		{
			// Data Buffer
			D3D11_BUFFER_DESC particleBufferDesc;
			particleBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
			particleBufferDesc.CPUAccessFlags = 0;
			particleBufferDesc.Usage = D3D11_USAGE_DEFAULT;
			particleBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
			particleBufferDesc.StructureByteStride = sizeof(float4);
			particleBufferDesc.ByteWidth = controlParticleCount * sizeof(float4);

			Egg11::ThrowOnFail("Could not create particleDataBuffer.", __FILE__, __LINE__) ^
				device->CreateBuffer(&particleBufferDesc, NULL, controlParticleNewPosDataBuffer.GetAddressOf());


			// Shader Resource View
			D3D11_SHADER_RESOURCE_VIEW_DESC particleSRVDesc;
			particleSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
			particleSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
			particleSRVDesc.Buffer.FirstElement = 0;
			particleSRVDesc.Buffer.NumElements = controlParticles.size();

			Egg11::ThrowOnFail("Could not create metaballVSParticleSRV.", __FILE__, __LINE__) ^
				device->CreateShaderResourceView(controlParticleNewPosDataBuffer.Get(), &particleSRVDesc, &controlParticleNewPosSRV);


			// Unordered Access View
			D3D11_UNORDERED_ACCESS_VIEW_DESC particleUAVDesc;
			particleUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
			particleUAVDesc.Format = DXGI_FORMAT_UNKNOWN;
			particleUAVDesc.Buffer.FirstElement = 0;
			particleUAVDesc.Buffer.NumElements = controlParticles.size();
			particleUAVDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_COUNTER; // WHY????

			Egg11::ThrowOnFail("Could not create animationUAV.", __FILE__, __LINE__) ^
				device->CreateUnorderedAccessView(controlParticleNewPosDataBuffer.Get(), &particleUAVDesc, &controlParticleNewPosUAV);
		}
		{
			// Data Buffer
			D3D11_BUFFER_DESC particleBufferDesc;
			particleBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
			particleBufferDesc.CPUAccessFlags = 0;
			particleBufferDesc.Usage = D3D11_USAGE_DEFAULT;
			particleBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
			particleBufferDesc.StructureByteStride = sizeof(float4);
			particleBufferDesc.ByteWidth = controlParticleCount * sizeof(float4);

			std::vector<float4> initVelocioty (controlParticleCount);
			D3D11_SUBRESOURCE_DATA initialParticleData;
			initialParticleData.pSysMem = &initVelocioty.at(0);

			Egg11::ThrowOnFail("Could not create particleDataBuffer.", __FILE__, __LINE__) ^
				device->CreateBuffer(&particleBufferDesc, NULL, controlParticleVelocityDataBuffer.GetAddressOf());


			// Shader Resource View
			D3D11_SHADER_RESOURCE_VIEW_DESC particleSRVDesc;
			particleSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
			particleSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
			particleSRVDesc.Buffer.FirstElement = 0;
			particleSRVDesc.Buffer.NumElements = controlParticles.size();

			Egg11::ThrowOnFail("Could not create metaballVSParticleSRV.", __FILE__, __LINE__) ^
				device->CreateShaderResourceView(controlParticleVelocityDataBuffer.Get(), &particleSRVDesc, &controlParticleVelocitySRV);


			// Unordered Access View
			D3D11_UNORDERED_ACCESS_VIEW_DESC particleUAVDesc;
			particleUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
			particleUAVDesc.Format = DXGI_FORMAT_UNKNOWN;
			particleUAVDesc.Buffer.FirstElement = 0;
			particleUAVDesc.Buffer.NumElements = controlParticles.size();
			particleUAVDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_COUNTER; // WHY????

			Egg11::ThrowOnFail("Could not create animationUAV.", __FILE__, __LINE__) ^
				device->CreateUnorderedAccessView(controlParticleVelocityDataBuffer.Get(), &particleUAVDesc, &controlParticleVelocityUAV);
		}

		{
			ComPtr<ID3DBlob> shaderByteCode = loadShaderCode("csPBDGravity.cso");
			PBDShaderGravity = Egg11::Mesh::Shader::create("csPBDGravity.cso", device, shaderByteCode);
		}
		{
			ComPtr<ID3DBlob> shaderByteCode = loadShaderCode("csPBDCollision.cso");
			PBDShaderCollision = Egg11::Mesh::Shader::create("csPBDCollision.cso", device, shaderByteCode);
		}
		{
			ComPtr<ID3DBlob> shaderByteCode = loadShaderCode("csPBDDistance.cso");
			PBDShaderDistance = Egg11::Mesh::Shader::create("csPBDDistance.cso", device, shaderByteCode);
		}
		{
			ComPtr<ID3DBlob> shaderByteCode = loadShaderCode("csPBDBending.cso");
			PBDShaderBending = Egg11::Mesh::Shader::create("csPBDBending.cso", device, shaderByteCode);
		}
		{
			ComPtr<ID3DBlob> shaderByteCode = loadShaderCode("csPBDFinalUpdate.cso");
			PBDShaderFinalUpdate = Egg11::Mesh::Shader::create("csPBDFinalUpdate.cso", device, shaderByteCode);
		}

	}

	{
		// ControlParticleCounter

		// Data Buffer
		D3D11_BUFFER_DESC particleCounterBufferDesc;
		particleCounterBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		particleCounterBufferDesc.CPUAccessFlags = 0;
		particleCounterBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		particleCounterBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
		particleCounterBufferDesc.StructureByteStride = sizeof(uint);
		particleCounterBufferDesc.ByteWidth = sizeof(uint);

		if (controlParticlePlacement = PBD) {
			uint32_t controlParticleCountInitData = controlParticleCount;
			D3D11_SUBRESOURCE_DATA initialData;
			initialData.pSysMem = &controlParticleCountInitData;

			Egg11::ThrowOnFail("Could not create particleDataBuffer.", __FILE__, __LINE__) ^
				device->CreateBuffer(&particleCounterBufferDesc, &initialData, controlParticleCounterDataBuffer.GetAddressOf());
		}
		else {
			Egg11::ThrowOnFail("Could not create particleDataBuffer.", __FILE__, __LINE__) ^
				device->CreateBuffer(&particleCounterBufferDesc, NULL, controlParticleCounterDataBuffer.GetAddressOf());
		}
		


		// Shader Resource View
		D3D11_SHADER_RESOURCE_VIEW_DESC particleCounterSRVDesc;
		particleCounterSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		particleCounterSRVDesc.Format = DXGI_FORMAT_R32_UINT;
		particleCounterSRVDesc.Buffer.FirstElement = 0;
		particleCounterSRVDesc.Buffer.NumElements = 1;

		Egg11::ThrowOnFail("Could not create metaballVSParticleSRV.", __FILE__, __LINE__) ^
			device->CreateShaderResourceView(controlParticleCounterDataBuffer.Get(), &particleCounterSRVDesc, &controlParticleCounterSRV);


		// Unordered Access View
		D3D11_UNORDERED_ACCESS_VIEW_DESC particleCounterUAVDesc;
		particleCounterUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		particleCounterUAVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		particleCounterUAVDesc.Buffer.FirstElement = 0;
		particleCounterUAVDesc.Buffer.NumElements = 1;
		particleCounterUAVDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW; // WHY????

		Egg11::ThrowOnFail("Could not create animationUAV.", __FILE__, __LINE__) ^
			device->CreateUnorderedAccessView(controlParticleCounterDataBuffer.Get(), &particleCounterUAVDesc, &controlParticleCounterUAV);
	}

	{
		// Only Debug

		D3D11_BUFFER_DESC particleCounterBufferDesc;
		particleCounterBufferDesc.BindFlags = 0;
		particleCounterBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		particleCounterBufferDesc.Usage = D3D11_USAGE_STAGING;
		particleCounterBufferDesc.MiscFlags = 0;
		particleCounterBufferDesc.StructureByteStride = sizeof(uint);
		particleCounterBufferDesc.ByteWidth = sizeof(uint);

		Egg11::ThrowOnFail("Could not create particleDataBuffer.", __FILE__, __LINE__) ^
			device->CreateBuffer(&particleCounterBufferDesc, NULL, uavCounterReadback.GetAddressOf());
	}

	{
		// ControlParticleIndirectDisptach

		// Data Buffer
		D3D11_BUFFER_DESC particleIndirectDisptachBufferDesc;
		particleIndirectDisptachBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		particleIndirectDisptachBufferDesc.CPUAccessFlags = 0;
		particleIndirectDisptachBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		particleIndirectDisptachBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
		particleIndirectDisptachBufferDesc.StructureByteStride = sizeof(uint);
		particleIndirectDisptachBufferDesc.ByteWidth = 3 * sizeof(uint);

		Egg11::ThrowOnFail("Could not create particleDataBuffer.", __FILE__, __LINE__) ^
			device->CreateBuffer(&particleIndirectDisptachBufferDesc, NULL, controlParticleIndirectDisptachDataBuffer.GetAddressOf());


		// Unordered Access View
		D3D11_UNORDERED_ACCESS_VIEW_DESC particleIndirectDisptachUAVDesc;
		particleIndirectDisptachUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		particleIndirectDisptachUAVDesc.Format = DXGI_FORMAT_R32_UINT;
		particleIndirectDisptachUAVDesc.Buffer.FirstElement = 0;
		particleIndirectDisptachUAVDesc.Buffer.NumElements = 3;
		particleIndirectDisptachUAVDesc.Buffer.Flags = 0;

		Egg11::ThrowOnFail("Could not create animationUAV.", __FILE__, __LINE__) ^
			device->CreateUnorderedAccessView(controlParticleIndirectDisptachDataBuffer.Get(), &particleIndirectDisptachUAVDesc, &controlParticleIndirectDisptachUAV);
	}
}

void Game::CreateBillboard() {
	using namespace Microsoft::WRL;

	// Vertex Input
	billboardNothing = Egg11::Mesh::Nothing::create(defaultParticleCount, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

	// billboardGSSizeCB
	D3D11_BUFFER_DESC billboardSizeCBDesc;
	billboardSizeCBDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	billboardSizeCBDesc.CPUAccessFlags = 0;
	billboardSizeCBDesc.MiscFlags = 0;
	billboardSizeCBDesc.StructureByteStride = 0;
	billboardSizeCBDesc.Usage = D3D11_USAGE_DEFAULT;
	billboardSizeCBDesc.ByteWidth = sizeof(Egg11::Math::float4) * 1;

	Egg11::Math::float4 billboardSize(.1, .1, 0, 0);
	D3D11_SUBRESOURCE_DATA initialBbSize;
	initialBbSize.pSysMem = &billboardSize;

	Egg11::ThrowOnFail("Failed to create billboardGSSizeCB.", __FILE__, __LINE__) ^
		device->CreateBuffer(&billboardSizeCBDesc, &initialBbSize, billboardSizeCB.GetAddressOf());

	// Shaders
	ComPtr<ID3DBlob> billboardVertexShaderByteCode = loadShaderCode("vsBillboard.cso");
	Egg11::Mesh::Shader::P billboardVertexShader = Egg11::Mesh::Shader::create("vsBillboard.cso", device, billboardVertexShaderByteCode);

	ComPtr<ID3DBlob> billboardGeometryShaderByteCode = loadShaderCode("gsBillboard.cso");
	Egg11::Mesh::Shader::P billboardGeometryShader = Egg11::Mesh::Shader::create("gsBillboard.cso", device, billboardGeometryShaderByteCode);

	ComPtr<ID3DBlob> billboardPixelShaderByteCode = loadShaderCode("psBillboard.cso");
	billboardsPixelShader = Egg11::Mesh::Shader::create("psBillboard.cso", device, billboardPixelShaderByteCode);

	ComPtr<ID3DBlob> billboardPixelShaderAByteCode = loadShaderCode("psBillboardA.cso");
	billboardsPixelShaderA = Egg11::Mesh::Shader::create("psBillboardA.cso", device, billboardPixelShaderAByteCode);

	ComPtr<ID3DBlob> billboardPixelShaderS1ByteCode = loadShaderCode("psBillboardS1.cso");
	billboardsPixelShaderS1 = Egg11::Mesh::Shader::create("psBillboardS1.cso", device, billboardPixelShaderS1ByteCode);

	ComPtr<ID3DBlob> billboardsPixelShaderS2ByteCode = loadShaderCode("psBillboardS2.cso");
	billboardsPixelShaderS2 = Egg11::Mesh::Shader::create("psBillboardS2.cso", device, billboardsPixelShaderS2ByteCode);

	ComPtr<ID3DBlob> billboardPixelShaderSV21ByteCode = loadShaderCode("psBillboardSV21.cso");
	billboardsPixelShaderSV21 = Egg11::Mesh::Shader::create("psBillboardSV21.cso", device, billboardPixelShaderSV21ByteCode);

	ComPtr<ID3DBlob> billboardsPixelShaderSV22ByteCode = loadShaderCode("psBillboardSV22.cso");
	billboardsPixelShaderSV22 = Egg11::Mesh::Shader::create("psBillboardSV22.cso", device, billboardsPixelShaderSV22ByteCode);

	Egg11::Mesh::Material::P billboardMaterial = Egg11::Mesh::Material::create();
	billboardMaterial->setShader(Egg11::Mesh::ShaderStageFlag::Vertex, billboardVertexShader);
	billboardMaterial->setShader(Egg11::Mesh::ShaderStageFlag::Geometry, billboardGeometryShader);
	//billboardMaterial->setShader(Egg11::Mesh::ShaderStageFlag::Pixel, billboardPixelShader);
	billboardMaterial->setCb("billboardGSTransCB", modelViewProjCB, Egg11::Mesh::ShaderStageFlag::Geometry);
	billboardMaterial->setCb("billboardGSSizeCB", billboardSizeCB, Egg11::Mesh::ShaderStageFlag::Geometry);

	// Depth settings
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> DSState;
	D3D11_DEPTH_STENCIL_DESC dsDesc;

	// Depth test parameters
	dsDesc.DepthEnable = true;
	dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	dsDesc.DepthFunc = D3D11_COMPARISON_LESS;

	// Stencil test parameters
	dsDesc.StencilEnable = false;
	dsDesc.StencilReadMask = 0xFF;
	dsDesc.StencilWriteMask = 0xFF;

	// Stencil operations if pixel is front-facing
	dsDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	dsDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	// Stencil operations if pixel is back-facing
	dsDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	dsDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
	dsDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	dsDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	// Create depth stencil state
	device->CreateDepthStencilState(&dsDesc, DSState.GetAddressOf());

	billboardMaterial->depthStencilState = DSState;
	ComPtr<ID3D11InputLayout> billboardInputLayout = inputBinder->getCompatibleInputLayout(billboardVertexShaderByteCode, billboardNothing);
	billboards = Egg11::Mesh::Shaded::create(billboardNothing, billboardMaterial, billboardInputLayout);

	// Create Offset Buffer
	D3D11_BUFFER_DESC offsetBufferDesc;
	ZeroMemory(&offsetBufferDesc, sizeof(D3D11_BUFFER_DESC));
	offsetBufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	offsetBufferDesc.StructureByteStride = sizeof(unsigned int);
	offsetBufferDesc.ByteWidth = windowHeight * windowWidth * offsetBufferDesc.StructureByteStride;
	offsetBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
	device->CreateBuffer(&offsetBufferDesc, NULL, &offsetBuffer);

	// Create Offset Buffer Shader Resource Views
	D3D11_SHADER_RESOURCE_VIEW_DESC offsetSRVDesc;
	offsetSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	offsetSRVDesc.Buffer.FirstElement = 0;
	offsetSRVDesc.Format = DXGI_FORMAT_R32_UINT;
	offsetSRVDesc.Buffer.NumElements = windowWidth * windowHeight;
	device->CreateShaderResourceView(offsetBuffer.Get(), &offsetSRVDesc, &offsetSRV);

	// Create Offset Buffer Unordered Access Views
	D3D11_UNORDERED_ACCESS_VIEW_DESC offsetUAVDesc;
	offsetUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	offsetUAVDesc.Buffer.FirstElement = 0;
	offsetUAVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	offsetUAVDesc.Buffer.NumElements = windowHeight * windowWidth;
	offsetUAVDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
	device->CreateUnorderedAccessView(offsetBuffer.Get(), &offsetUAVDesc, &offsetUAV);


	// Create Link Buffer.
	D3D11_BUFFER_DESC linkBufferDesc;
	ZeroMemory(&linkBufferDesc, sizeof(D3D11_BUFFER_DESC));
	linkBufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	linkBufferDesc.StructureByteStride = sizeof(unsigned int) * 2;
	linkBufferDesc.ByteWidth = windowHeight * windowWidth * linkbufferSizePerPixel * linkBufferDesc.StructureByteStride;
	linkBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	device->CreateBuffer(&linkBufferDesc, NULL, &linkBuffer);

	// Create Link Buffer Shader Resource Views
	D3D11_SHADER_RESOURCE_VIEW_DESC linkSRVDesc;
	linkSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	linkSRVDesc.Buffer.FirstElement = 0;
	linkSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
	linkSRVDesc.Buffer.NumElements = windowWidth * windowHeight * linkbufferSizePerPixel;
	device->CreateShaderResourceView(linkBuffer.Get(), &linkSRVDesc, &linkSRV);

	// Create Link Buffer Unordered Access Views
	D3D11_UNORDERED_ACCESS_VIEW_DESC linkUAVDesc;
	linkUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	linkUAVDesc.Buffer.FirstElement = 0;
	linkUAVDesc.Format = DXGI_FORMAT_UNKNOWN;
	linkUAVDesc.Buffer.NumElements = windowHeight * windowWidth * linkbufferSizePerPixel;
	linkUAVDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_COUNTER;
	device->CreateUnorderedAccessView(linkBuffer.Get(), &linkUAVDesc, &linkUAV);


	// Create id Buffer
	D3D11_BUFFER_DESC idBufferDesc;
	ZeroMemory(&idBufferDesc, sizeof(D3D11_BUFFER_DESC));
	idBufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	idBufferDesc.StructureByteStride = sizeof(unsigned int);
	idBufferDesc.ByteWidth = windowHeight * windowWidth * sbufferSizePerPixel * idBufferDesc.StructureByteStride;
	idBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	device->CreateBuffer(&idBufferDesc, NULL, &idBuffer);

	// Create id Buffer Shader Resource Views
	D3D11_SHADER_RESOURCE_VIEW_DESC idSRVDesc;
	idSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	idSRVDesc.Buffer.FirstElement = 0;
	idSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
	idSRVDesc.Buffer.NumElements = windowWidth * windowHeight * sbufferSizePerPixel;
	device->CreateShaderResourceView(idBuffer.Get(), &idSRVDesc, &idSRV);

	// Create id Buffer Unordered Access Views
	D3D11_UNORDERED_ACCESS_VIEW_DESC idUAVDesc;
	idUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	idUAVDesc.Buffer.FirstElement = 0;
	idUAVDesc.Format = DXGI_FORMAT_UNKNOWN;
	idUAVDesc.Buffer.NumElements = windowHeight * windowWidth * sbufferSizePerPixel;
	idUAVDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_COUNTER;
	device->CreateUnorderedAccessView(idBuffer.Get(), &idUAVDesc, &idUAV);


	// Create Count Buffer
	D3D11_BUFFER_DESC countBufferDesc;
	ZeroMemory(&countBufferDesc, sizeof(D3D11_BUFFER_DESC));
	countBufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	countBufferDesc.StructureByteStride = sizeof(unsigned int);
	countBufferDesc.ByteWidth = windowHeight * windowWidth * countBufferDesc.StructureByteStride;
	countBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
	device->CreateBuffer(&countBufferDesc, NULL, &countBuffer);

	// Create Count Buffer Unordered Access Views
	D3D11_UNORDERED_ACCESS_VIEW_DESC countUAVDesc;
	countUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	countUAVDesc.Buffer.FirstElement = 0;
	countUAVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	countUAVDesc.Buffer.NumElements = windowHeight * windowWidth;
	countUAVDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
	device->CreateUnorderedAccessView(countBuffer.Get(), &countUAVDesc, &countUAV);

	// Create Counter Buffer
	D3D11_BUFFER_DESC counterBufferDesc;
	ZeroMemory(&counterBufferDesc, sizeof(D3D11_BUFFER_DESC));
	counterBufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	counterBufferDesc.StructureByteStride = sizeof(unsigned int);
	counterBufferDesc.ByteWidth = counterSize * counterBufferDesc.StructureByteStride;
	counterBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
	device->CreateBuffer(&counterBufferDesc, NULL, &counterBuffer);

	// Create Counter Buffer Shader Resource Views
	D3D11_SHADER_RESOURCE_VIEW_DESC counterSRVDesc;
	counterSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	counterSRVDesc.Buffer.FirstElement = 0;
	counterSRVDesc.Format = DXGI_FORMAT_R32_UINT;
	counterSRVDesc.Buffer.NumElements = counterSize;
	device->CreateShaderResourceView(counterBuffer.Get(), &counterSRVDesc, &counterSRV);

	// Create Counter Buffer Unordered Access Views
	D3D11_UNORDERED_ACCESS_VIEW_DESC counterUAVDesc;
	counterUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	counterUAVDesc.Buffer.FirstElement = 0;
	counterUAVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	counterUAVDesc.Buffer.NumElements = counterSize;
	counterUAVDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
	device->CreateUnorderedAccessView(counterBuffer.Get(), &counterUAVDesc, &counterUAV);

}

void Game::CreateBillboardForControlParticles() {
	using namespace Microsoft::WRL;

	// Vertex Input
	cpBillboardNothing = Egg11::Mesh::Nothing::create(controlParticleCount, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

	// billboardGSSizeCB
	D3D11_BUFFER_DESC billboardSizeCBDesc;
	billboardSizeCBDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	billboardSizeCBDesc.CPUAccessFlags = 0;
	billboardSizeCBDesc.MiscFlags = 0;
	billboardSizeCBDesc.StructureByteStride = 0;
	billboardSizeCBDesc.Usage = D3D11_USAGE_DEFAULT;
	billboardSizeCBDesc.ByteWidth = sizeof(Egg11::Math::float4) * 1;

	Egg11::Math::float4 billboardSize(.1, .1, 0, 0);
	D3D11_SUBRESOURCE_DATA initialBbSize;
	initialBbSize.pSysMem = &billboardSize;

	Egg11::ThrowOnFail("Failed to create billboardGSSizeCB.", __FILE__, __LINE__) ^
		device->CreateBuffer(&billboardSizeCBDesc, &initialBbSize, cpBillboardSizeCB.GetAddressOf());

	// Shaders
	ComPtr<ID3DBlob> billboardVertexShaderByteCode = loadShaderCode("vsBillboardControl.cso");
	Egg11::Mesh::Shader::P billboardVertexShader = Egg11::Mesh::Shader::create("vsBillboard.cso", device, billboardVertexShaderByteCode);

	ComPtr<ID3DBlob> billboardGeometryShaderByteCode = loadShaderCode("gsBillboard.cso");
	Egg11::Mesh::Shader::P billboardGeometryShader = Egg11::Mesh::Shader::create("gsBillboard.cso", device, billboardGeometryShaderByteCode);

	Egg11::Mesh::Material::P billboardMaterial = Egg11::Mesh::Material::create();
	billboardMaterial->setShader(Egg11::Mesh::ShaderStageFlag::Vertex, billboardVertexShader);
	billboardMaterial->setShader(Egg11::Mesh::ShaderStageFlag::Geometry, billboardGeometryShader);
	//billboardMaterial->setShader(Egg11::Mesh::ShaderStageFlag::Pixel, billboardPixelShader);
	billboardMaterial->setCb("billboardGSTransCB", modelViewProjCB, Egg11::Mesh::ShaderStageFlag::Geometry);
	billboardMaterial->setCb("billboardGSSizeCB", billboardSizeCB, Egg11::Mesh::ShaderStageFlag::Geometry);


	// Depth settings
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> DSState;
	D3D11_DEPTH_STENCIL_DESC dsDesc;

	// Depth test parameters
	dsDesc.DepthEnable = true;
	dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	dsDesc.DepthFunc = D3D11_COMPARISON_LESS;

	// Stencil test parameters
	dsDesc.StencilEnable = false;
	dsDesc.StencilReadMask = 0xFF;
	dsDesc.StencilWriteMask = 0xFF;

	// Stencil operations if pixel is front-facing
	dsDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	dsDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	// Stencil operations if pixel is back-facing
	dsDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	dsDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
	dsDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	dsDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	// Create depth stencil state
	device->CreateDepthStencilState(&dsDesc, DSState.GetAddressOf());

	billboardMaterial->depthStencilState = DSState;

	ComPtr<ID3D11InputLayout> billboardInputLayout = inputBinder->getCompatibleInputLayout(billboardVertexShaderByteCode, cpBillboardNothing);
	cpBillboards = Egg11::Mesh::Shaded::create(cpBillboardNothing, billboardMaterial, billboardInputLayout);


}

void Game::CreatePrefixSum() {
	using namespace Microsoft::WRL;

	// scanBucketSizeCB
	D3D11_BUFFER_DESC scanBucketSizeCBDesc;
	scanBucketSizeCBDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	scanBucketSizeCBDesc.CPUAccessFlags = 0;
	scanBucketSizeCBDesc.MiscFlags = 0;
	scanBucketSizeCBDesc.StructureByteStride = 0;
	scanBucketSizeCBDesc.Usage = D3D11_USAGE_DEFAULT;
	scanBucketSizeCBDesc.ByteWidth = sizeof(Egg11::Math::int1) * 4;

	Egg11::Math::int1 scanSize(0);
	D3D11_SUBRESOURCE_DATA initialScanSize;
	initialScanSize.pSysMem = &scanSize;

	Egg11::ThrowOnFail("Failed to create scanBucketSizeCB.", __FILE__, __LINE__) ^
		device->CreateBuffer(&scanBucketSizeCBDesc, &initialScanSize, scanBucketSizeCB.GetAddressOf());


	// Create Result Buffer
	D3D11_BUFFER_DESC resultBufferDesc;
	ZeroMemory(&resultBufferDesc, sizeof(D3D11_BUFFER_DESC));
	resultBufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	resultBufferDesc.StructureByteStride = sizeof(unsigned int);
	resultBufferDesc.ByteWidth = windowHeight * windowWidth * resultBufferDesc.StructureByteStride;
	resultBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
	device->CreateBuffer(&resultBufferDesc, NULL, &resultBuffer);

	// Create Unordered Access Views
	D3D11_UNORDERED_ACCESS_VIEW_DESC resultUAVDesc;
	resultUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	resultUAVDesc.Buffer.FirstElement = 0;
	resultUAVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	resultUAVDesc.Buffer.NumElements = windowHeight * windowWidth;
	resultUAVDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
	device->CreateUnorderedAccessView(resultBuffer.Get(), &resultUAVDesc, &resultUAV);


	// Compute shaders
	ComPtr<ID3DBlob> computeShaderByteCode = loadShaderCode("csPrefixSum.cso");
	prefixSumComputeShader = Egg11::Mesh::Shader::create("csPrefixSum.cso", device, computeShaderByteCode);

	ComPtr<ID3DBlob> computeShaderByteCode2 = loadShaderCode("csScanBucketResult.cso");
	prefixSumScanBucketResultShader = Egg11::Mesh::Shader::create("csScanBucketResult.cso", device, computeShaderByteCode2);

	ComPtr<ID3DBlob> computeShaderByteCode3 = loadShaderCode("csScanAddBucketResult.cso");
	prefixSumScanAddBucketResultShader = Egg11::Mesh::Shader::create("csScanAddBucketResult.cso", device, computeShaderByteCode3);

	ComPtr<ID3DBlob> computeShaderByteCode4 = loadShaderCode("csPrefixSumV2.cso");
	prefixSumV2ComputeShader = Egg11::Mesh::Shader::create("csPrefixSumV2.cso", device, computeShaderByteCode4);
}

void Game::CreateEnviroment()
{
	using namespace Microsoft::WRL;

	//Environment texture
	Microsoft::WRL::ComPtr<ID3D11Resource> envTexture;

	std::wstring envfile = Egg11::UtfConverter::utf8to16(App::getSystemEnvironment().resolveMediaPath("cloudynoon.dds"));
	Egg11::ThrowOnFail("Could not create ENV.", __FILE__, __LINE__) ^
		DirectX::CreateDDSTextureFromFile(device.Get(), envfile.c_str(), envTexture.GetAddressOf(), envSrv.GetAddressOf());

	CD3D11_SAMPLER_DESC samplerStateDesc = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());
	device->CreateSamplerState(&samplerStateDesc, samplerState.GetAddressOf());
}

void Game::CreateMetaball() {
	using namespace Microsoft::WRL;

	// Shaders
	Egg11::Mesh::Geometry::P fullQuadGeometry = Egg11::Mesh::Indexed::createQuad(device);

	ComPtr<ID3DBlob> metaballVertexShaderByteCode = loadShaderCode("vsMetaball.cso");
	Egg11::Mesh::Shader::P metaballVertexShader = Egg11::Mesh::Shader::create("vsMetaball.cso", device, metaballVertexShaderByteCode);

	ComPtr<ID3DBlob> metaballRealisticPixelShaderByteCode = loadShaderCode("psMetaBallNormalRealistic.cso");
	metaballRealisticPixelShader = Egg11::Mesh::Shader::create("psMetaBallNormalRealistic.cso", device, metaballRealisticPixelShaderByteCode);

	ComPtr<ID3DBlob> metaballRealisticAPixelShaderByteCode = loadShaderCode("psMetaballABufferRealistic.cso");
	metaballRealisticAPixelShader = Egg11::Mesh::Shader::create("psMetaballABufferRealistic.cso", device, metaballRealisticAPixelShaderByteCode);

	ComPtr<ID3DBlob> metaballRealisticSPixelShaderByteCode = loadShaderCode("psMetaBallSBufferRealistic.cso");
	metaballRealisticSPixelShader = Egg11::Mesh::Shader::create("psMetaBallSBufferRealistic.cso", device, metaballRealisticSPixelShaderByteCode);

	ComPtr<ID3DBlob> metaballRealisticS2PixelShaderByteCode = loadShaderCode("psMetaBallS2BufferRealistic.cso");
	metaballRealisticS2PixelShader = Egg11::Mesh::Shader::create("psMetaBallS2BufferRealistic.cso", device, metaballRealisticS2PixelShaderByteCode);

	ComPtr<ID3DBlob> metaballGradientPixelShaderByteCode = loadShaderCode("psMetaBallNormalGradient.cso");
	metaballGradientPixelShader = Egg11::Mesh::Shader::create("psMetaBallNormalGradient.cso", device, metaballGradientPixelShaderByteCode);

	ComPtr<ID3DBlob> metaballGradientAPixelShaderByteCode = loadShaderCode("psMetaballABufferGradient.cso");
	metaballGradientAPixelShader = Egg11::Mesh::Shader::create("psMetaballABufferGradient.cso", device, metaballGradientAPixelShaderByteCode);

	ComPtr<ID3DBlob> metaballGradientSPixelShaderByteCode = loadShaderCode("psMetaBallSBufferGradient.cso");
	metaballGradientSPixelShader = Egg11::Mesh::Shader::create("psMetaBallSBufferGradient.cso", device, metaballGradientSPixelShaderByteCode);

	ComPtr<ID3DBlob> metaballGradientS2PixelShaderByteCode = loadShaderCode("psMetaBallS2BufferGradient.cso");
	metaballGradientS2PixelShader = Egg11::Mesh::Shader::create("psMetaBallS2BufferGradient.cso", device, metaballGradientS2PixelShaderByteCode);

	ComPtr<ID3DBlob> metaballRealisticHashSimpleShaderByteCode = loadShaderCode("psMetaballHashSimpleRealistic.cso");
	metaballRealisticHashSimpleShader = Egg11::Mesh::Shader::create("psMetaballHashSimpleRealistic.cso", device, metaballRealisticHashSimpleShaderByteCode);

	ComPtr<ID3DBlob> metaballGradientHashSimpleShaderByteCode = loadShaderCode("psMetaballHashSimpleGradient.cso");
	metaballGradientHashSimpleShader = Egg11::Mesh::Shader::create("psMetaballHashSimpleGradient.cso", device, metaballGradientHashSimpleShaderByteCode);

	Egg11::Mesh::Material::P metaballMaterial = Egg11::Mesh::Material::create();
	metaballMaterial->setShader(Egg11::Mesh::ShaderStageFlag::Vertex, metaballVertexShader);
	//metaballMaterial->setShader(Egg11::Mesh::ShaderStageFlag::Pixel, backgroundPixelShader);
	metaballMaterial->setCb("metaballVSTransCB", modelViewProjCB, Egg11::Mesh::ShaderStageFlag::Vertex);
	//metaballMaterial->setCb("metaballPSEyePosCB", eyePosCB, Egg11::Mesh::ShaderStageFlag::Pixel);
	//metaballMaterial->setCb("shadingCB", shadingCB, Egg11::Mesh::ShaderStageFlag::Pixel);
	//metaballMaterial->setSamplerState("ss", samplerState, Egg11::Mesh::ShaderStageFlag::Pixel);

	// Depth settings
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> DSState;
	D3D11_DEPTH_STENCIL_DESC dsDesc;

	// Depth test parameters
	dsDesc.DepthEnable = false;
	dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	dsDesc.DepthFunc = D3D11_COMPARISON_LESS;

	// Stencil test parameters
	dsDesc.StencilEnable = false;
	dsDesc.StencilReadMask = 0xFF;
	dsDesc.StencilWriteMask = 0xFF;

	// Stencil operations if pixel is front-facing
	dsDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	dsDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	// Stencil operations if pixel is back-facing
	dsDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	dsDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
	dsDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	dsDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	// Create depth stencil state
	device->CreateDepthStencilState(&dsDesc, DSState.GetAddressOf());

	metaballMaterial->depthStencilState = DSState;


	ComPtr<ID3D11InputLayout>metaballInputLayout = inputBinder->getCompatibleInputLayout(metaballVertexShaderByteCode, fullQuadGeometry);
	metaballs = Egg11::Mesh::Shaded::create(fullQuadGeometry, metaballMaterial, metaballInputLayout);
}

void Game::CreateAnimation() {
	using namespace Microsoft::WRL;

	ComPtr<ID3DBlob>fluidSimulationShaderByteCode = loadShaderCode("csFluidSimulation.cso");
	fluidSimulationShader = Egg11::Mesh::Shader::create("csFluidSimulation.cso", device, fluidSimulationShaderByteCode);

	ComPtr<ID3DBlob> controlledFluidSimulationShaderByteCode = loadShaderCode("csControlledFluidSimulation.cso");
	controlledFluidSimulationShader = Egg11::Mesh::Shader::create("csControlledFluidSimulation.cso", device, controlledFluidSimulationShaderByteCode);

	ComPtr<ID3DBlob>simpleSortEvenShaderByteCode = loadShaderCode("csSimpleSortEven.cso");
	simpleSortEvenShader = Egg11::Mesh::Shader::create("csSimpleSortEven.cso", device, simpleSortEvenShaderByteCode);

	ComPtr<ID3DBlob>simpleSortOddShaderByteCode = loadShaderCode("csSimpleSortOdd.cso");
	simpleSortOddShader = Egg11::Mesh::Shader::create("csSimpleSortOdd.cso", device, simpleSortOddShaderByteCode);

	ComPtr<ID3DBlob>mortonHashShaderByteCode = loadShaderCode("csMortonHash.cso");
	mortonHashShader = Egg11::Mesh::Shader::create("csMortonHash.cso", device, mortonHashShaderByteCode);

	ComPtr<ID3DBlob>setIndirectDispatchBufferShaderByteCode = loadShaderCode("csSetBufferForIndirectDispatch.cso");
	setIndirectDispatchBufferShader = Egg11::Mesh::Shader::create("csSetBufferForIndirectDispatch.cso", device, setIndirectDispatchBufferShaderByteCode);

	ComPtr<ID3DBlob>adaptiveControlPressureShaderByteCode = loadShaderCode("csAdaptiveControlPressure.cso");
	adaptiveControlPressureShader = Egg11::Mesh::Shader::create("csAdaptiveControlPressure.cso", device, adaptiveControlPressureShaderByteCode);

	ComPtr<ID3DBlob> rigControlParticlesShaderByteCode = loadShaderCode("csRigControlParticles.cso");
	rigControlParticlesShader = Egg11::Mesh::Shader::create("csRigControlParticles.cso", device, rigControlParticlesShaderByteCode);

	ComPtr<ID3DBlob>animateControlParticlesShaderByteCode = loadShaderCode("csAnimateControlParticles.cso");
	animateControlParticlesShader = Egg11::Mesh::Shader::create("csAnimateControlParticles.cso", device, animateControlParticlesShaderByteCode);

	controlParams = { 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f };

	// debugTypeCB
	D3D11_BUFFER_DESC controlParamsCBDesc;
	controlParamsCBDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	controlParamsCBDesc.CPUAccessFlags = 0;
	controlParamsCBDesc.MiscFlags = 0;
	controlParamsCBDesc.StructureByteStride = 0;
	controlParamsCBDesc.Usage = D3D11_USAGE_DEFAULT;
	controlParamsCBDesc.ByteWidth = sizeof(float) * 8;

	D3D11_SUBRESOURCE_DATA initialControlParamsData;
	initialControlParamsData.pSysMem = &controlParams[0];

	Egg11::ThrowOnFail("Failed to create debugTypeCB.", __FILE__, __LINE__) ^
		device->CreateBuffer(&controlParamsCBDesc, &initialControlParamsData, controlParamsCB.GetAddressOf());
}

void Game::CreateControlMesh() {
	Assimp::Importer importer;

	//const aiScene* assScene = importer.ReadFile(App::getSystemEnvironment().resolveMediaPath("mrem.dae"), 0);
	//meshIdxInFile = 1;

	//const aiScene* assScene = importer.ReadFile(App::getSystemEnvironment().resolveMediaPath("aj.dae"), 0);

	//const aiScene* assScene = importer.ReadFile(App::getSystemEnvironment().resolveMediaPath("castle_guard_02.dae"), 0);

	//const aiScene* assScene = importer.ReadFile(App::getSystemEnvironment().resolveMediaPath("Samba Dancing2.dae"), 0);
	//animatedControlMeshScale = 0.003;

	const aiScene* assScene = importer.ReadFile(App::getSystemEnvironment().resolveMediaPath("Jazz Dancing.dae"), 0);
	animatedControlMeshScale = 0.003;

	meshIdxInFile = 0;


	// if the import failed
	if (!assScene || !assScene->HasMeshes() || assScene->mNumMeshes == 0)
	{
		return;
	}

	//aiMesh* assMesh = assScene->mMeshes[1];
	aiMesh* assMesh = assScene->mMeshes[meshIdxInFile];
	Egg11::Mesh::Indexed::P indexedMesh = Egg11::Mesh::Importer::fromAiMesh(device, assMesh);

	nBones = assMesh->mNumBones;
	rigging = new DualQuaternion[nBones];

	std::vector<Egg11::Math::float4> bonePositions(nBones);

	for (int iBone = 0; iBone< assMesh->mNumBones; iBone++) {
		aiBone* assBone = assMesh->mBones[iBone];
		boneNames.push_back(assBone->mName.C_Str());
		aiMatrix4x4& a = assBone->mOffsetMatrix;
		float4x4 m(
			a.a1, a.a2, a.a3, a.a4,
			a.b1, a.b2, a.b3, a.b4,
			a.c1, a.c2, a.c3, a.c4,
			a.d1, a.d2, a.d3, a.d4
		);
		aiQuaternion q;
		aiVector3D t;
		a.DecomposeNoScaling(q, t);
		rigging[
			//riggingPoseBoneTransforms.size()
			iBone].set(float4(q.x, q.y, q.z, q.w), float4(t.x, t.y, t.z, 1.0));

			bonePositions[iBone] = float4(-t.x, -t.y, -t.z, 1.0);
			//		riggingPoseBoneTransforms.push_back(m);
			boneTransformationChainNodeIndices.push_back(std::vector<unsigned char>());
	}

	Assimp::Importer importer2;
	//const aiScene* assAnimScene = importer2.ReadFile(App::getSystemEnvironment().resolveMediaPath("thriller_part_3.dae"), 0);	
	const aiScene* assAnimScene = importer2.ReadFile(App::getSystemEnvironment().resolveMediaPath("Jazz Dancing.dae"), 0);
	aiAnimation* assAnim = assAnimScene->mAnimations[0];
	skeleton = new unsigned char[nBones * 16];
	memset(skeleton, 0xff, nBones * 16);
	struct NodeProcessor {
		Game* game;
		unsigned char* skeleton;
		unsigned int iNode;
		NodeProcessor(Game* game, unsigned char* skeleton) : game(game), skeleton(skeleton), iNode(0) {}
		void process(aiNode* assNode) {
			game->nodeNamesToNodeIndices[assNode->mName.C_Str()] = iNode;
			iNode++;
			//game->nodeOffsetTransforms.push_back( m );
			//aiMatrix4x4& a = assNode->mTransformation;
			for (int iBone = 0; iBone < game->boneNames.size(); iBone++) {
				if (game->boneNames[iBone] == assNode->mName.C_Str()) {

					aiNode* pNode = assNode;
					while (pNode) {
						std::map<std::string, unsigned char>::iterator iNodeIndex = game->nodeNamesToNodeIndices.find(pNode->mName.C_Str());
						if (iNodeIndex != game->nodeNamesToNodeIndices.end()) {
							skeleton[iBone * 16 + game->boneTransformationChainNodeIndices[iBone].size()] = iNodeIndex->second;
							game->boneTransformationChainNodeIndices[iBone].push_back(iNodeIndex->second);
						}
						else
						{
							// never happens
						}
						pNode = pNode->mParent;
					}
					break;
				}
			}

			for (int iChild = 0; iChild<assNode->mNumChildren; iChild++) {
				process(assNode->mChildren[iChild]);
			}
		}
	} np(this, skeleton);
	np.process(assAnimScene->mRootNode);

	//nKeys = 768;
	nKeys = 61;
	nNodes = np.iNode;//nodeOffsetTransforms.size();

	keys = new DualQuaternion[
		nNodes
			* nKeys
	];

	std::map<std::string, unsigned char>::iterator iNodeIndex = nodeNamesToNodeIndices.begin();
	std::map<std::string, unsigned char>::iterator eNodeIndex = nodeNamesToNodeIndices.end();
	while (iNodeIndex != eNodeIndex) {
		aiNode* assNode = assAnimScene->mRootNode->FindNode(iNodeIndex->first.c_str());
		aiQuaternion q;
		aiVector3D t;
		assNode->mTransformation.DecomposeNoScaling(q, t);
		DualQuaternion dq(
			float4(q.x, q.y, q.z, q.w),
			float4(t.x, t.y, t.z, 1));
		for (int iKey = 0; iKey< nKeys; iKey++) {
			keys[iNodeIndex->second * nKeys + iKey] = dq;
		}
		iNodeIndex++;
	}

	for (int iAnim = 0; iAnim<assAnimScene->mNumAnimations; iAnim++) {
		aiAnimation* assAnim = assAnimScene->mAnimations[iAnim];
		for (int iChannel = 0; iChannel< assAnim->mNumChannels; iChannel++) {
			aiNodeAnim* assChannel = assAnim->mChannels[iChannel];
			std::map<std::string, unsigned char>::iterator iNodeIndex = nodeNamesToNodeIndices.find(assChannel->mNodeName.C_Str());
			if (iNodeIndex != nodeNamesToNodeIndices.end()) {
				for (int iKey = 0; iKey< assChannel->mNumPositionKeys; iKey++) {
					aiVector3D& p = assChannel->mPositionKeys[iKey].mValue;
					aiQuaternion& q = assChannel->mRotationKeys[iKey].mValue;
					keys[iNodeIndex->second * nKeys + iKey] = DualQuaternion(
						float4(q.x, q.y, q.z, q.w),
						float4(p.x, p.y, p.z, 1));
				}
			}
			else
			{
				//never happens
			}
		}
	}


	CD3D11_BUFFER_DESC boneBufferDesc(
		nBones *
		2 *
		sizeof(float4)
		, D3D11_BIND_CONSTANT_BUFFER);
	boneBufferDesc.CPUAccessFlags = 0;
	boneBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	boneBufferDesc.StructureByteStride = 0;
	Egg11::ThrowOnFail("Failed to create boneCB.", __FILE__, __LINE__) ^
		device->CreateBuffer(&boneBufferDesc, NULL, boneBuffer.GetAddressOf());

	using namespace Microsoft::WRL;

	Egg11::Mesh::Geometry::P geometry = Egg11::Mesh::Importer::fromAiMesh(device, assScene->mMeshes[meshIdxInFile]);

	ComPtr<ID3DBlob> vertexShaderByteCode = loadShaderCode("vsSkinning.cso");
	Egg11::Mesh::Shader::P vertexShader = Egg11::Mesh::Shader::create("vsSkinning.cso", device, vertexShaderByteCode);

	ComPtr<ID3DBlob> pixelShaderByteCode = loadShaderCode("psControlMeshA.cso");
	Egg11::Mesh::Shader::P pixelShader = Egg11::Mesh::Shader::create("psControlMeshA.cso", device, pixelShaderByteCode);

	//ComPtr<ID3DBlob> pixelShaderByteCode = loadShaderCode("psIdle.cso");
	//Egg11::Mesh::Shader::P pixelShader = Egg11::Mesh::Shader::create("psIdle.cso", device, pixelShaderByteCode);

	Egg11::Mesh::Material::P material = Egg11::Mesh::Material::create();
	material->setShader(Egg11::Mesh::ShaderStageFlag::Vertex, vertexShader);
	material->setShader(Egg11::Mesh::ShaderStageFlag::Pixel, pixelShader);
	material->setCb("modelViewProjCB", modelViewProjCB, Egg11::Mesh::ShaderStageFlag::Vertex);
	material->setCb("boneCB", boneBuffer, Egg11::Mesh::ShaderStageFlag::Vertex);
	material->setCb("modelViewProjCB", modelViewProjCB, Egg11::Mesh::ShaderStageFlag::Pixel);


	// Depth settings
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> DSState;
	D3D11_DEPTH_STENCIL_DESC dsDesc;

	// Depth test parameters
	dsDesc.DepthEnable = false;
	dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	dsDesc.DepthFunc = D3D11_COMPARISON_LESS;

	// Stencil test parameters
	dsDesc.StencilEnable = false;
	dsDesc.StencilReadMask = 0xFF;
	dsDesc.StencilWriteMask = 0xFF;

	// Stencil operations if pixel is front-facing
	dsDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	dsDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	// Stencil operations if pixel is back-facing
	dsDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	dsDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
	dsDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	dsDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	// Create depth stencil state
	device->CreateDepthStencilState(&dsDesc, DSState.GetAddressOf());
	material->depthStencilState = DSState;

	/// Raster settings
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> RasterizerState;

	D3D11_RASTERIZER_DESC RasterizerDesc;
	RasterizerDesc.CullMode = D3D11_CULL_NONE;
	RasterizerDesc.FillMode = D3D11_FILL_SOLID;
	RasterizerDesc.FrontCounterClockwise = FALSE;
	RasterizerDesc.DepthBias = D3D11_DEFAULT_DEPTH_BIAS;
	RasterizerDesc.DepthBiasClamp = D3D11_DEFAULT_DEPTH_BIAS_CLAMP;
	RasterizerDesc.SlopeScaledDepthBias = D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	RasterizerDesc.DepthClipEnable = TRUE;
	RasterizerDesc.ScissorEnable = FALSE;
	RasterizerDesc.MultisampleEnable = FALSE;
	RasterizerDesc.AntialiasedLineEnable = FALSE;

	device->CreateRasterizerState(&RasterizerDesc, RasterizerState.GetAddressOf());
	material->rasterizerState = RasterizerState;

	ComPtr<ID3D11InputLayout> inputLayout = inputBinder->getCompatibleInputLayout(vertexShaderByteCode, geometry);
	animatedControlMesh = Egg11::Mesh::Shaded::create(geometry, material, inputLayout);

	currentKey = 0;


	//Debug
	{
		ComPtr<ID3DBlob> pixelShaderByteCodeDebug = loadShaderCode("psFlat.cso");
		Egg11::Mesh::Shader::P pixelShaderDebug = Egg11::Mesh::Shader::create("psFlat.cso", device, pixelShaderByteCodeDebug);

		Egg11::Mesh::Material::P materialDebug = Egg11::Mesh::Material::create();
		materialDebug->setShader(Egg11::Mesh::ShaderStageFlag::Vertex, vertexShader);
		materialDebug->setShader(Egg11::Mesh::ShaderStageFlag::Pixel, pixelShaderDebug);
		materialDebug->setCb("modelViewProjCB", modelViewProjCB, Egg11::Mesh::ShaderStageFlag::Vertex);
		materialDebug->setCb("boneCB", boneBuffer, Egg11::Mesh::ShaderStageFlag::Vertex);
		//materialDebug->setCb("modelViewProjCB", modelViewProjCB, Egg11::Mesh::ShaderStageFlag::Pixel);

		ComPtr<ID3D11InputLayout> inputLayoutDebug = inputBinder->getCompatibleInputLayout(vertexShaderByteCode, geometry);
		animatedControlMeshFlat = Egg11::Mesh::Shaded::create(geometry, materialDebug, inputLayoutDebug);
	}



	for (int i = 0; i < nBones; i++)
	{
		bonePositions[i] = bonePositions[i] * (float4x4::scaling(float3(animatedControlMeshScale, animatedControlMeshScale, animatedControlMeshScale)));
		//bonePositions[i].w = 1.0;
	}

	D3D11_BUFFER_DESC bonePositionsDesc;
	bonePositionsDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bonePositionsDesc.CPUAccessFlags = 0;
	bonePositionsDesc.MiscFlags = 0;
	bonePositionsDesc.StructureByteStride = 0;
	bonePositionsDesc.Usage = D3D11_USAGE_DEFAULT;
	bonePositionsDesc.ByteWidth = sizeof(float) * 4 * nBones;

	D3D11_SUBRESOURCE_DATA initialBonePositionData;
	initialBonePositionData.pSysMem = &bonePositions[0];

	Egg11::ThrowOnFail("Failed to create bonePositionseCB.", __FILE__, __LINE__) ^
		device->CreateBuffer(&bonePositionsDesc, &initialBonePositionData, bonePositionsBufferCB.GetAddressOf());


}

void Game::CreateDebug()
{
	using namespace Microsoft::WRL;

	ComPtr<ID3DBlob>controlParticleBallPixelShaderByteCode = loadShaderCode("psControlParticleBall.cso");
	controlParticleBallPixelShader = Egg11::Mesh::Shader::create("psControlParticleBall.cso", device, controlParticleBallPixelShaderByteCode);

	ComPtr<ID3DBlob>particleBallPixelShaderByteCode = loadShaderCode("psParticleBall.cso");
	particleBallPixelShader = Egg11::Mesh::Shader::create("psParticleBall.cso", device, particleBallPixelShaderByteCode);

	// debugTypeCB
	D3D11_BUFFER_DESC debugTypeCBDesc;
	debugTypeCBDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	debugTypeCBDesc.CPUAccessFlags = 0;
	debugTypeCBDesc.MiscFlags = 0;
	debugTypeCBDesc.StructureByteStride = 0;
	debugTypeCBDesc.Usage = D3D11_USAGE_DEFAULT;
	debugTypeCBDesc.ByteWidth = sizeof(float) * 4;

	uint debugTypeData[4] = { 1, 0, 0, 0 };
	D3D11_SUBRESOURCE_DATA initialDebugTypeData;
	initialDebugTypeData.pSysMem = &debugTypeData;

	Egg11::ThrowOnFail("Failed to create debugTypeCB.", __FILE__, __LINE__) ^
		device->CreateBuffer(&debugTypeCBDesc, &initialDebugTypeData, debugTypeCB.GetAddressOf());
}

HRESULT Game::releaseResources()
{
	billboards.reset();
	return Egg11::App::releaseResources();
}

void Game::clearRenderTarget(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context) {
	context->OMSetRenderTargets(1, defaultRenderTargetView.GetAddressOf(), defaultDepthStencilView.Get());

	float clearColor[4] = { 0.9f, 0.7f, 0.1f, 0.0f };
	context->ClearRenderTargetView(defaultRenderTargetView.Get(), clearColor);
	context->ClearDepthStencilView(defaultDepthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0, 0);
}

void Game::clearContext(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
{
	// new code from skeleton branch 
	/*UINT pNumViewports = 1;
	D3D11_VIEWPORT pViewports[1];
	context->RSGetViewports(&pNumViewports, pViewports);
	context->ClearState();
	context->RSSetViewports(pNumViewports, pViewports);
	context->OMSetRenderTargets(1, defaultRenderTargetView.GetAddressOf(), defaultDepthStencilView.Get());*/

	context->ClearState();

	D3D11_VIEWPORT pViewports[1];
	pViewports->Height = windowHeight;
	pViewports->Width = windowWidth;
	pViewports->TopLeftX = 0;
	pViewports->TopLeftY = 0;
	pViewports->MaxDepth = 1.0;
	pViewports->MinDepth = 0.0;
	context->RSSetViewports(1, pViewports);

	context->OMSetRenderTargets(1, defaultRenderTargetView.GetAddressOf(), defaultDepthStencilView.Get());

}

void Game::renderParticleBillboard(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
{
	float4x4 matrices[4];
	matrices[0] = float4x4::identity;
	matrices[1] = (firstPersonCam->getViewMatrix() * firstPersonCam->getProjMatrix()).invert();
	matrices[2] = (firstPersonCam->getViewMatrix() * firstPersonCam->getProjMatrix());
	matrices[3] = firstPersonCam->getViewDirMatrix();
	context->UpdateSubresource(modelViewProjCB.Get(), 0, nullptr, matrices, 0, 0);

	float4 perFrameVectors[1];
	perFrameVectors[0] = firstPersonCam->getEyePosition().xyz1;
	context->UpdateSubresource(eyePosCB.Get(), 0, nullptr, perFrameVectors, 0, 0);

	context->VSSetShaderResources(0, 1, particleSRV.GetAddressOf());

	billboards->getMaterial()->setCb("billboardGSMatricesCB", modelViewProjCB, Egg11::Mesh::ShaderStageFlag::Vertex);

	billboards->getMaterial()->setShader(Egg11::Mesh::ShaderStageFlag::Pixel, billboardsPixelShader);

	billboards->draw(context);

}

void Game::renderControlParticleBillboard(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
{
	float4x4 matrices[4];
	matrices[0] = float4x4::identity;
	matrices[1] = (firstPersonCam->getViewMatrix() * firstPersonCam->getProjMatrix()).invert();
	matrices[2] = (firstPersonCam->getViewMatrix() * firstPersonCam->getProjMatrix());
	matrices[3] = firstPersonCam->getViewDirMatrix();
	context->UpdateSubresource(modelViewProjCB.Get(), 0, nullptr, matrices, 0, 0);

	float4 perFrameVectors[1];
	perFrameVectors[0] = firstPersonCam->getEyePosition().xyz1;
	context->UpdateSubresource(eyePosCB.Get(), 0, nullptr, perFrameVectors, 0, 0);

	context->VSSetShaderResources(0, 1, controlParticleSRV.GetAddressOf());

	cpBillboards->getMaterial()->setCb("billboardGSMatricesCB", modelViewProjCB, Egg11::Mesh::ShaderStageFlag::Vertex);

	cpBillboards->getMaterial()->setShader(Egg11::Mesh::ShaderStageFlag::Pixel, billboardsPixelShader);

	cpBillboards->draw(context);

}

void Game::renderBillboardA(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
{
	uint values[4] = { 0,0,0,0 };
	context->ClearUnorderedAccessViewUint(offsetUAV.Get(), values);

	float4x4 matrices[4];
	matrices[0] = float4x4::identity;
	matrices[1] = (firstPersonCam->getViewMatrix() * firstPersonCam->getProjMatrix()).invert();
	matrices[2] = (firstPersonCam->getViewMatrix() * firstPersonCam->getProjMatrix());
	matrices[3] = firstPersonCam->getViewDirMatrix();
	context->UpdateSubresource(modelViewProjCB.Get(), 0, nullptr, matrices, 0, 0);
	context->VSSetShaderResources(0, 1, particleSRV.GetAddressOf());

	ID3D11UnorderedAccessView* ppUnorderedAccessViews[2];
	ppUnorderedAccessViews[0] = offsetUAV.Get();
	ppUnorderedAccessViews[1] = linkUAV.Get();
	uint t[2] = { 0,0 };
	context->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, defaultDepthStencilView.Get(), 0, 2, ppUnorderedAccessViews, t);

	billboards->getMaterial()->setShader(Egg11::Mesh::ShaderStageFlag::Pixel, billboardsPixelShaderA);

	billboards->draw(context);

}

void Game::renderBillboardS1(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
{
	uint values[4] = { 0,0,0,0 };
	context->ClearUnorderedAccessViewUint(offsetUAV.Get(), values);

	float4x4 matrices[4];
	matrices[0] = float4x4::identity;
	matrices[1] = (firstPersonCam->getViewMatrix() * firstPersonCam->getProjMatrix()).invert();
	matrices[2] = (firstPersonCam->getViewMatrix() * firstPersonCam->getProjMatrix());
	matrices[3] = firstPersonCam->getViewDirMatrix();
	context->UpdateSubresource(modelViewProjCB.Get(), 0, nullptr, matrices, 0, 0);
	context->VSSetShaderResources(0, 1, particleSRV.GetAddressOf());

	ID3D11UnorderedAccessView* ppUnorderedAccessViews[2];
	ppUnorderedAccessViews[0] = offsetUAV.Get();
	uint t[1] = { 0 };
	context->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, defaultDepthStencilView.Get(), 0, 1, ppUnorderedAccessViews, t);

	billboards->getMaterial()->setShader(Egg11::Mesh::ShaderStageFlag::Pixel, billboardsPixelShaderS1);

	billboards->draw(context);
}

void Game::renderBillboardSV21(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
{
	uint values[4] = { 0,0,0,0 };
	context->ClearUnorderedAccessViewUint(offsetUAV.Get(), values);
	context->ClearUnorderedAccessViewUint(counterUAV.Get(), values);

	float4x4 matrices[4];
	matrices[0] = float4x4::identity;
	matrices[1] = (firstPersonCam->getViewMatrix() * firstPersonCam->getProjMatrix()).invert();
	matrices[2] = (firstPersonCam->getViewMatrix() * firstPersonCam->getProjMatrix());
	matrices[3] = firstPersonCam->getViewDirMatrix();
	context->UpdateSubresource(modelViewProjCB.Get(), 0, nullptr, matrices, 0, 0);
	context->VSSetShaderResources(0, 1, particleSRV.GetAddressOf());

	ID3D11UnorderedAccessView* ppUnorderedAccessViews[2];
	ppUnorderedAccessViews[0] = offsetUAV.Get();
	ppUnorderedAccessViews[1] = counterUAV.Get();
	uint t[2] = { 0, 0 };
	context->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, defaultDepthStencilView.Get(), 0, 2, ppUnorderedAccessViews, t);

	billboards->getMaterial()->setShader(Egg11::Mesh::ShaderStageFlag::Pixel, billboardsPixelShaderSV21);

	billboards->draw(context);
}

void Game::renderBillboardS2(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
{
	float4x4 matrices[4];
	matrices[0] = float4x4::identity;
	matrices[1] = (firstPersonCam->getViewMatrix() * firstPersonCam->getProjMatrix()).invert();
	matrices[2] = (firstPersonCam->getViewMatrix() * firstPersonCam->getProjMatrix());
	matrices[3] = firstPersonCam->getViewDirMatrix();
	context->UpdateSubresource(modelViewProjCB.Get(), 0, nullptr, matrices, 0, 0);

	context->VSSetShaderResources(0, 1, particleSRV.GetAddressOf());
	ID3D11UnorderedAccessView* ppUnorderedAccessViews[3];
	ppUnorderedAccessViews[0] = offsetUAV.Get();
	ppUnorderedAccessViews[1] = countUAV.Get();
	ppUnorderedAccessViews[2] = idUAV.Get();
	uint t[3] = { 0,0,0 };
	context->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, defaultDepthStencilView.Get(), 0, 3, ppUnorderedAccessViews, t);

	billboards->getMaterial()->setShader(Egg11::Mesh::ShaderStageFlag::Pixel, billboardsPixelShaderS2);

	billboards->draw(context);
}

void Game::renderBillboardSV22(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
{
	float4x4 matrices[4];
	matrices[0] = float4x4::identity;
	matrices[1] = (firstPersonCam->getViewMatrix() * firstPersonCam->getProjMatrix()).invert();
	matrices[2] = (firstPersonCam->getViewMatrix() * firstPersonCam->getProjMatrix());
	matrices[3] = firstPersonCam->getViewDirMatrix();
	context->UpdateSubresource(modelViewProjCB.Get(), 0, nullptr, matrices, 0, 0);

	context->VSSetShaderResources(0, 1, particleSRV.GetAddressOf());
	ID3D11UnorderedAccessView* ppUnorderedAccessViews[3];
	ppUnorderedAccessViews[0] = offsetUAV.Get();
	ppUnorderedAccessViews[1] = countUAV.Get();
	ppUnorderedAccessViews[2] = idUAV.Get();
	uint t[3] = { 0,0,0 };
	context->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, defaultDepthStencilView.Get(), 0, 3, ppUnorderedAccessViews, t);

	billboards->getMaterial()->setShader(Egg11::Mesh::ShaderStageFlag::Pixel, billboardsPixelShaderSV22);

	billboards->draw(context);
}

void Game::renderMetaball(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context) {
	float4x4 matrices[4];
	matrices[0] = float4x4::identity;
	//matrices[1] = (firstPersonCam->getViewMatrix() * firstPersonCam->getProjMatrix()).invert();
	matrices[1] = (firstPersonCam->getViewMatrix());
	matrices[2] = (firstPersonCam->getViewMatrix() * firstPersonCam->getProjMatrix());
	matrices[3] = firstPersonCam->getViewDirMatrix();
	context->UpdateSubresource(modelViewProjCB.Get(), 0, nullptr, matrices, 0, 0);

	float4 perFrameVectors[1];
	perFrameVectors[0] = firstPersonCam->getEyePosition().xyz1;
	context->UpdateSubresource(eyePosCB.Get(), 0, nullptr, perFrameVectors, 0, 0);
	context->UpdateSubresource(modelViewProjCB.Get(), 0, nullptr, matrices, 0, 0);

	float4 shadingAttributes[8];
	shadingAttributes[0] = float4(0.4, 0.0, 0.0, 1.0); //ambientIntensity
	shadingAttributes[1] = float4(1.0, 0.0, 0.0, 1.0); //lightDir
	shadingAttributes[2] = float4(0.250, 0.129, 0.027, 1.0); // surfaceColor
	shadingAttributes[3] = float4(1.0, 1.0, 1.0, 1.0); // lightColor

	if (metalShading == Gold)
	{
		shadingAttributes[4] = float4(0.17, 0.38, 1.5, 0.0); //eta
		shadingAttributes[5] = float4(3.7, 2.45, 1.85, 0.0); //kappa
	}
	if (metalShading == Copper)
	{
		shadingAttributes[4] = float4(0.11, 0.8, 1.07, 0.0); //eta
		shadingAttributes[5] = float4(3.9, 2.72, 2.5, 0.0); //kappa
	}
	if (metalShading == Aluminium)
	{
		shadingAttributes[4] = float4(1.49, 1.02, 0.558, 0.0); //eta
		shadingAttributes[5] = float4(7.82, 6.85, 5.2, 0.0); //kappa
	}
	shadingAttributes[6] = float4(metaBallMinToHit, 0.0, 0.0, 0.0);
	shadingAttributes[7] = radius;
	context->UpdateSubresource(shadingCB.Get(), 0, nullptr, shadingAttributes, 0, 0);

	int type[5];
	if (shading == PhongShading)
		type[0] = 1;
	if (shading == MetalShading)
		type[0] = 2;
	if (waterShading == DeepWater)
		type[1] = 1;
	if (waterShading == SimpleWater)
		type[1] = 0;
	type[2] = binaryStepCount;
	type[3] = maxRecursion;
	type[4] = marchCount;

	context->UpdateSubresource(shadingTypeCB.Get(), 0, nullptr, type, 0, 0);

	int mfunctionType[1];
	if (metaballFunction == Simple)
		mfunctionType[0] = 1;
	if (metaballFunction == Wyvill)
		mfunctionType[0] = 2;
	if (metaballFunction == Nishimura)
		mfunctionType[0] = 3;
	if (metaballFunction == Murakami)
		mfunctionType[0] = 4;

	context->UpdateSubresource(metaballFunctionCB.Get(), 0, nullptr, mfunctionType, 0, 0);

	context->PSSetShaderResources(0, 1, envSrv.GetAddressOf());
	context->PSSetShaderResources(1, 1, particleSRV.GetAddressOf());
	if (billboardsLoadAlgorithm == ABuffer || billboardsLoadAlgorithm == SBuffer || billboardsLoadAlgorithm == SBufferV2)
		context->PSSetShaderResources(2, 1, offsetSRV.GetAddressOf());
	if (billboardsLoadAlgorithm == ABuffer)
		context->PSSetShaderResources(3, 1, linkSRV.GetAddressOf());
	if (billboardsLoadAlgorithm == SBuffer || billboardsLoadAlgorithm == SBufferV2)
		context->PSSetShaderResources(3, 1, idSRV.GetAddressOf());

	if (billboardsLoadAlgorithm == HashSimple) {
		//uint values[4] = { 0,0,0,0 };
		//context->ClearUnorderedAccessViewUint(hlistBeginUAV.Get(), values);

		ID3D11UnorderedAccessView* ppUnorderedAccessViews[4];
		ppUnorderedAccessViews[0] = hlistBeginUAV.Get();
		ppUnorderedAccessViews[1] = hlistLengthUAV.Get();
		ppUnorderedAccessViews[2] = clistBeginUAV.Get();
		ppUnorderedAccessViews[3] = clistLengthUAV.Get();
		uint t[4] = { 0, 0, 0, 0 };
		context->OMSetRenderTargetsAndUnorderedAccessViews(D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL, NULL, NULL, 1, 4, ppUnorderedAccessViews, t);
	}

	switch (billboardsLoadAlgorithm)
	{
	case Normal:
	{
		if (renderMode == Realistic)
			metaballs->getMaterial()->setShader(Egg11::Mesh::ShaderStageFlag::Pixel, metaballRealisticPixelShader);
		else
			metaballs->getMaterial()->setShader(Egg11::Mesh::ShaderStageFlag::Pixel, metaballGradientPixelShader);
		break;
	}
	case ABuffer:
	{
		if (renderMode == Realistic)
			metaballs->getMaterial()->setShader(Egg11::Mesh::ShaderStageFlag::Pixel, metaballRealisticAPixelShader);
		else
			metaballs->getMaterial()->setShader(Egg11::Mesh::ShaderStageFlag::Pixel, metaballGradientAPixelShader);
		break;
	}
	case SBuffer:
	{
		if (renderMode == Realistic)
			metaballs->getMaterial()->setShader(Egg11::Mesh::ShaderStageFlag::Pixel, metaballRealisticSPixelShader);
		else
			metaballs->getMaterial()->setShader(Egg11::Mesh::ShaderStageFlag::Pixel, metaballGradientSPixelShader);
		break;
	}
	case SBufferV2:
	{
		if (renderMode == Realistic)
			metaballs->getMaterial()->setShader(Egg11::Mesh::ShaderStageFlag::Pixel, metaballRealisticS2PixelShader);
		else
			metaballs->getMaterial()->setShader(Egg11::Mesh::ShaderStageFlag::Pixel, metaballGradientS2PixelShader);
		break;
	}
	case HashSimple:
	{
		if (renderMode == Realistic)
			metaballs->getMaterial()->setShader(Egg11::Mesh::ShaderStageFlag::Pixel, metaballRealisticHashSimpleShader);
		else
			metaballs->getMaterial()->setShader(Egg11::Mesh::ShaderStageFlag::Pixel, metaballGradientHashSimpleShader);
		break;
	}
	default:
	{
		throw("Mateball No Shader");
		break;
	}
	}

	metaballs->getMaterial()->setCb("metaballVSTransCB", modelViewProjCB, Egg11::Mesh::ShaderStageFlag::Vertex);
	metaballs->getMaterial()->setCb("metaballPSEyePosCB", eyePosCB, Egg11::Mesh::ShaderStageFlag::Pixel);
	metaballs->getMaterial()->setCb("metaballVSTransCB", modelViewProjCB, Egg11::Mesh::ShaderStageFlag::Pixel);
	metaballs->getMaterial()->setCb("shadingCB", shadingCB, Egg11::Mesh::ShaderStageFlag::Pixel);
	metaballs->getMaterial()->setCb("shadingTypeCB", shadingTypeCB, Egg11::Mesh::ShaderStageFlag::Pixel);
	metaballs->getMaterial()->setCb("metaballFunctionCB", metaballFunctionCB, Egg11::Mesh::ShaderStageFlag::Pixel);
	metaballs->getMaterial()->setSamplerState("ss", samplerState, Egg11::Mesh::ShaderStageFlag::Pixel);

	metaballs->draw(context);
}

void Game::renderControlBalls(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context) {
	float4x4 matrices[4];
	matrices[0] = float4x4::identity;
	matrices[1] = float4x4::identity;
	matrices[2] = (firstPersonCam->getViewMatrix() * firstPersonCam->getProjMatrix());
	matrices[3] = firstPersonCam->getViewDirMatrix();
	context->UpdateSubresource(modelViewProjCB.Get(), 0, nullptr, matrices, 0, 0);

	float4 perFrameVectors[1];
	perFrameVectors[0] = firstPersonCam->getEyePosition().xyz1;
	context->UpdateSubresource(eyePosCB.Get(), 0, nullptr, perFrameVectors, 0, 0);

	context->PSSetShaderResources(0, 1, envSrv.GetAddressOf());
	context->PSSetShaderResources(1, 1, controlParticleSRV.GetAddressOf());
	context->PSSetShaderResources(2, 1, controlParticleCounterSRV.GetAddressOf());

	metaballs->getMaterial()->setShader(Egg11::Mesh::ShaderStageFlag::Pixel, controlParticleBallPixelShader);


	metaballs->getMaterial()->setCb("metaballVSTransCB", modelViewProjCB, Egg11::Mesh::ShaderStageFlag::Vertex);
	metaballs->getMaterial()->setCb("metaballPSEyePosCB", eyePosCB, Egg11::Mesh::ShaderStageFlag::Pixel);
	metaballs->getMaterial()->setCb("metaballVSTransCB", modelViewProjCB, Egg11::Mesh::ShaderStageFlag::Pixel);
	metaballs->getMaterial()->setSamplerState("ss", samplerState, Egg11::Mesh::ShaderStageFlag::Pixel);

	metaballs->draw(context);
}

void Game::renderBalls(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context) {
	float4x4 matrices[4];
	matrices[0] = float4x4::identity;
	matrices[1] = (firstPersonCam->getViewMatrix() * firstPersonCam->getProjMatrix()).invert();
	matrices[2] = (firstPersonCam->getViewMatrix() * firstPersonCam->getProjMatrix());
	matrices[3] = firstPersonCam->getViewDirMatrix();
	context->UpdateSubresource(modelViewProjCB.Get(), 0, nullptr, matrices, 0, 0);

	float4 perFrameVectors[1];
	perFrameVectors[0] = firstPersonCam->getEyePosition().xyz1;
	context->UpdateSubresource(eyePosCB.Get(), 0, nullptr, perFrameVectors, 0, 0);

	context->PSSetShaderResources(0, 1, envSrv.GetAddressOf());
	context->PSSetShaderResources(1, 1, particleSRV.GetAddressOf());

	metaballs->getMaterial()->setShader(Egg11::Mesh::ShaderStageFlag::Pixel, particleBallPixelShader);


	metaballs->getMaterial()->setCb("metaballVSTransCB", modelViewProjCB, Egg11::Mesh::ShaderStageFlag::Vertex);
	metaballs->getMaterial()->setCb("metaballPSEyePosCB", eyePosCB, Egg11::Mesh::ShaderStageFlag::Pixel);
	metaballs->getMaterial()->setCb("metaballVSTransCB", modelViewProjCB, Egg11::Mesh::ShaderStageFlag::Pixel);
	metaballs->getMaterial()->setSamplerState("ss", samplerState, Egg11::Mesh::ShaderStageFlag::Pixel);
	metaballs->getMaterial()->setCb("debugTypeCB", debugTypeCB, Egg11::Mesh::ShaderStageFlag::Pixel);

	uint debugTypeTemp[4] = { debugType, 0, 0, 0 };
	context->UpdateSubresource(debugTypeCB.Get(), 0, nullptr, debugTypeTemp, 0, 0);

	metaballs->draw(context);
}

void Game::renderAnimation(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context) {
	if (flowControl == RealisticFlow)
	{
		uint zeros[2] = { 0, 0 };
		context->CSSetShader(static_cast<ID3D11ComputeShader*>(fluidSimulationShader->getShader().Get()), nullptr, 0);
		context->CSSetUnorderedAccessViews(0, 1, particleUAV.GetAddressOf(), zeros);
		context->Dispatch(defaultParticleCount, 1, 1);
	}
	else if (flowControl == ControlledFlow)
	{
		uint zeros[1] = { 0 };
		context->CSSetShader(static_cast<ID3D11ComputeShader*>(controlledFluidSimulationShader->getShader().Get()), nullptr, 0);
		context->CSSetUnorderedAccessViews(0, 1, particleUAV.GetAddressOf(), zeros);
		ID3D11ShaderResourceView* ppShaderResourceViews[2] = { controlParticleSRV.Get(), controlParticleCounterSRV.Get() };
		context->CSSetShaderResources(0, 2, ppShaderResourceViews);
		context->UpdateSubresource(controlParamsCB.Get(), 0, nullptr, &controlParams[0], 0, 0);
		uint cbindex = controlledFluidSimulationShader->getResourceIndex("controlParamsCB");
		context->CSSetConstantBuffers(cbindex, 1, controlParamsCB.GetAddressOf());
		context->Dispatch(defaultParticleCount, 1, 1);
	}
}

void Game::renderSort(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context) {
	uint zeros[2] = { 0, 0 };

	context->CSSetShader(static_cast<ID3D11ComputeShader*>(mortonHashShader->getShader().Get()), nullptr, 0);
	context->CSSetUnorderedAccessViews(0, 1, particleUAV.GetAddressOf(), zeros);
	context->Dispatch(defaultParticleCount, 1, 1);

	for (int i = 0; i < defaultParticleCount / 2; i++)
	{
		context->CSSetShader(static_cast<ID3D11ComputeShader*>(simpleSortEvenShader->getShader().Get()), nullptr, 0);
		context->CSSetUnorderedAccessViews(0, 1, particleUAV.GetAddressOf(), zeros);
		context->Dispatch(defaultParticleCount / 2, 1, 1);

		context->CSSetShader(static_cast<ID3D11ComputeShader*>(simpleSortOddShader->getShader().Get()), nullptr, 0);
		context->CSSetUnorderedAccessViews(0, 1, particleUAV.GetAddressOf(), zeros);
		context->Dispatch(defaultParticleCount / 2 - 1, 1, 1);
	}

}

void Game::renderInitCList(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context) {
	uint zeros[2] = { 0, 0 };

	context->CSSetShader(static_cast<ID3D11ComputeShader*>(clistShaderInit->getShader().Get()), nullptr, 0);
	context->CSSetShaderResources(0, 1, particleSRV.GetAddressOf());

	ID3D11UnorderedAccessView* ppUnorderedAccessViews[2];
	ppUnorderedAccessViews[0] = clistUAV.Get();
	ppUnorderedAccessViews[1] = clistLengthUAV.Get();
	context->CSSetUnorderedAccessViews(0, 2, ppUnorderedAccessViews, zeros);

	context->Dispatch(defaultParticleCount, 1, 1);
}

void Game::renderNonZeroPrefix(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context) {
	uint zeros[1] = { 0 };

	context->CSSetShader(static_cast<ID3D11ComputeShader*>(prefixSumComputeShader->getShader().Get()), nullptr, 0);
	context->CSSetUnorderedAccessViews(0, 1, clistLengthUAV.GetAddressOf(), zeros);
	context->Dispatch((int)ceil((defaultParticleCount + 1 / 512.0)), 1, 1);

	int loopCount = (int)ceil(((defaultParticleCount + 1) / 512.0) / 512.0);

	for (int i = 0; i < loopCount; i++)
	{
		context->CSSetShader(static_cast<ID3D11ComputeShader*>(prefixSumScanBucketResultShader->getShader().Get()), nullptr, 0);
		context->CSSetUnorderedAccessViews(0, 1, clistLengthUAV.GetAddressOf(), zeros);
		context->CSSetUnorderedAccessViews(1, 1, clistBeginUAV.GetAddressOf(), zeros);
		Egg11::Math::int1 sizeVector[1];
		sizeVector[0] = i;
		context->UpdateSubresource(scanBucketSizeCB.Get(), 0, nullptr, sizeVector, 0, 0);
		context->CSSetConstantBuffers(0, 1, scanBucketSizeCB.GetAddressOf());
		context->Dispatch(1, 1, 1);
	}

	context->CSSetShader(static_cast<ID3D11ComputeShader*>(prefixSumScanAddBucketResultShader->getShader().Get()), nullptr, 0);
	context->CSSetUnorderedAccessViews(0, 1, clistLengthUAV.GetAddressOf(), zeros);
	context->CSSetUnorderedAccessViews(1, 1, clistBeginUAV.GetAddressOf(), zeros);
	context->Dispatch((int)ceil((defaultParticleCount + 1 / 512.0)), 1, 1);

	resultBuffer.Reset();
}

void Game::renderCompactCList(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context) {
	uint values[4] = { 0,0,0,0 };
	context->ClearUnorderedAccessViewUint(clistBeginUAV.Get(), values);

	uint zeros[2] = { 0, 0 };

	ID3D11UnorderedAccessView* ppUnorderedAccessViews[4];
	ppUnorderedAccessViews[0] = clistUAV.Get();
	ppUnorderedAccessViews[1] = clistLengthUAV.Get();
	ppUnorderedAccessViews[2] = clistBeginUAV.Get();
	ppUnorderedAccessViews[3] = clistCellCountUAV.Get();

	context->CSSetShader(static_cast<ID3D11ComputeShader*>(clistShaderCompact->getShader().Get()), nullptr, 0);
	context->CSSetUnorderedAccessViews(0, 4, ppUnorderedAccessViews, zeros);
	context->Dispatch(defaultParticleCount, 1, 1);
}

void Game::renderLengthCList(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context) {
	uint values[4] = { 0,0,0,0 };
	context->ClearUnorderedAccessViewUint(clistLengthUAV.Get(), values);

	uint zeros[2] = { 0, 0 };

	ID3D11UnorderedAccessView* ppUnorderedAccessViews[3];
	ppUnorderedAccessViews[0] = clistBeginUAV.Get();
	ppUnorderedAccessViews[1] = clistLengthUAV.Get();
	ppUnorderedAccessViews[2] = clistCellCountUAV.Get();

	context->CSSetShader(static_cast<ID3D11ComputeShader*>(clistShaderLength->getShader().Get()), nullptr, 0);
	context->CSSetUnorderedAccessViews(0, 3, ppUnorderedAccessViews, zeros);
	context->Dispatch(defaultParticleCount, 1, 1);
}

void Game::renderInitHList(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context) {
	uint values[4] = { 0,0,0,0 };
	context->ClearUnorderedAccessViewUint(hlistUAV.Get(), values);

	uint zeros[2] = { 0, 0 };
	
	ID3D11UnorderedAccessView* ppUnorderedAccessViews[3];
	ppUnorderedAccessViews[0] = clistBeginUAV.Get();
	ppUnorderedAccessViews[1] = clistCellCountUAV.Get();
	ppUnorderedAccessViews[2] = hlistUAV.Get();

	context->CSSetShader(static_cast<ID3D11ComputeShader*>(hlistShaderInit->getShader().Get()), nullptr, 0);
	context->CSSetShaderResources(0, 1, particleSRV.GetAddressOf());
	context->CSSetUnorderedAccessViews(0, 3, ppUnorderedAccessViews, zeros);
	context->Dispatch(defaultParticleCount, 1, 1);
}

void Game::renderSortCList(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context) {
	uint zeros[2] = { 0, 0 };

	ID3D11UnorderedAccessView* ppUnorderedAccessViews[4];
	ppUnorderedAccessViews[0] = clistBeginUAV.Get();
	ppUnorderedAccessViews[1] = clistLengthUAV.Get();
	ppUnorderedAccessViews[2] = clistCellCountUAV.Get();
	ppUnorderedAccessViews[3] = hlistUAV.Get();

	for (int i = 0; i < defaultParticleCount / 2; i++)
	{
		context->CSSetShader(static_cast<ID3D11ComputeShader*>(clistShaderSortEven->getShader().Get()), nullptr, 0);
		context->CSSetUnorderedAccessViews(0, 4, ppUnorderedAccessViews, zeros);
		context->Dispatch(defaultParticleCount / 2, 1, 1);

		context->CSSetShader(static_cast<ID3D11ComputeShader*>(clistShaderSortOdd->getShader().Get()), nullptr, 0);
		context->CSSetUnorderedAccessViews(0, 4, ppUnorderedAccessViews, zeros);
		context->Dispatch(defaultParticleCount / 2 - 1, 1, 1);
	}
}

void Game::renderBeginHList(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context) {
	uint zeros[2] = { 0, 0 };

	ID3D11UnorderedAccessView* ppUnorderedAccessViews[4];
	ppUnorderedAccessViews[0] = clistCellCountUAV.Get();
	ppUnorderedAccessViews[1] = hlistUAV.Get();
	ppUnorderedAccessViews[2] = hlistBeginUAV.Get();
	ppUnorderedAccessViews[3] = hlistLengthUAV.Get();

	context->CSSetShader(static_cast<ID3D11ComputeShader*>(hlistShaderBegin->getShader().Get()), nullptr, 0);
	context->CSSetUnorderedAccessViews(0, 4, ppUnorderedAccessViews, zeros);
	context->Dispatch(defaultParticleCount, 1, 1);
}

void Game::renderLengthHList(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context) {
	uint zeros[2] = { 0, 0 };

	ID3D11UnorderedAccessView* ppUnorderedAccessViews[3];
	ppUnorderedAccessViews[0] = hlistBeginUAV.Get();
	ppUnorderedAccessViews[1] = hlistLengthUAV.Get();
	ppUnorderedAccessViews[2] = clistCellCountUAV.Get();

	context->CSSetShader(static_cast<ID3D11ComputeShader*>(hlistShaderLength->getShader().Get()), nullptr, 0);
	context->CSSetUnorderedAccessViews(0, 3, ppUnorderedAccessViews, zeros);
	context->Dispatch(hashCount, 1, 1);
}

void Game::renderPrefixSum(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
{
	uint zeros[1] = { 0 };

	context->CSSetShader(static_cast<ID3D11ComputeShader*>(prefixSumComputeShader->getShader().Get()), nullptr, 0);
	context->CSSetUnorderedAccessViews(0, 1, offsetUAV.GetAddressOf(), zeros);
	context->Dispatch((int)ceil((windowHeight * windowWidth / 512.0)), 1, 1);

	int loopCount = (int)ceil(((windowHeight*windowWidth) / 512.0) / 512.0);

	for (int i = 0; i < loopCount; i++)
	{
		context->CSSetShader(static_cast<ID3D11ComputeShader*>(prefixSumScanBucketResultShader->getShader().Get()), nullptr, 0);
		context->CSSetUnorderedAccessViews(0, 1, offsetUAV.GetAddressOf(), zeros);
		context->CSSetUnorderedAccessViews(1, 1, resultUAV.GetAddressOf(), zeros);
		Egg11::Math::int1 sizeVector[1];
		sizeVector[0] = i;
		context->UpdateSubresource(scanBucketSizeCB.Get(), 0, nullptr, sizeVector, 0, 0);
		context->CSSetConstantBuffers(0, 1, scanBucketSizeCB.GetAddressOf());
		context->Dispatch(1, 1, 1);
	}

	context->CSSetShader(static_cast<ID3D11ComputeShader*>(prefixSumScanAddBucketResultShader->getShader().Get()), nullptr, 0);
	context->CSSetUnorderedAccessViews(0, 1, offsetUAV.GetAddressOf(), zeros);
	context->CSSetUnorderedAccessViews(1, 1, resultUAV.GetAddressOf(), zeros);
	context->Dispatch((int)ceil((windowHeight * windowWidth / 512.0)), 1, 1);

	resultBuffer.Reset();
}

void Game::renderPrefixSumV2(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
{
	uint zeros[1] = { 0 };

	context->CSSetShader(static_cast<ID3D11ComputeShader*>(prefixSumComputeShader->getShader().Get()), nullptr, 0);
	context->CSSetUnorderedAccessViews(0, 1, counterUAV.GetAddressOf(), zeros);
	context->Dispatch(1, 1, 1);

	context->CSSetShader(static_cast<ID3D11ComputeShader*>(prefixSumV2ComputeShader->getShader().Get()), nullptr, 0);
	context->CSSetUnorderedAccessViews(0, 1, offsetUAV.GetAddressOf(), zeros);
	context->CSSetUnorderedAccessViews(1, 1, counterUAV.GetAddressOf(), zeros);
	context->Dispatch(counterSize, 1, 1);
}

void Game::renderControlMesh(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
{
	uint values[4] = { 0,0,0,0 };
	context->ClearUnorderedAccessViewUint(offsetUAV.Get(), values);

	float4x4 matrices[4];
	matrices[0] = float4x4::identity;
	matrices[1] = float4x4::identity;
	matrices[2] = float4x4::scaling(float3(controlMeshScale, controlMeshScale, controlMeshScale)) * fillCam->getViewMatrix()/* * fillCam->getProjMatrix()*/;
	matrices[3] = float4x4::identity;
	context->UpdateSubresource(modelViewProjCB.Get(), 0, nullptr, matrices, 0, 0);

	ID3D11UnorderedAccessView* ppUnorderedAccessViews[2];
	ppUnorderedAccessViews[0] = offsetUAV.Get();
	ppUnorderedAccessViews[1] = linkUAV.Get();
	uint t[2] = { 0,0 };
	context->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, defaultDepthStencilView.Get(), 0, 2, ppUnorderedAccessViews, t);

	D3D11_VIEWPORT pViewports[1];
	pViewports->Height = fillWindowHeight;
	pViewports->Width = fillWindowWidth;
	pViewports->TopLeftX = 0;
	pViewports->TopLeftY = 0;
	pViewports->MaxDepth = 1.0;
	pViewports->MinDepth = 0.0;
	context->RSSetViewports(1, pViewports);

	controlMesh->draw(context);

}

void Game::renderAnimatedControlMesh(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
{
	uint values[4] = { 0,0,0,0 };
	context->ClearUnorderedAccessViewUint(offsetUAV.Get(), values);

	float4x4 matrices[4];
	matrices[0] = float4x4::identity;
	matrices[1] = float4x4::identity;
	matrices[2] = float4x4::scaling(float3(animatedControlMeshScale, animatedControlMeshScale, animatedControlMeshScale))* float4x4::translation(float3(0.0, 0.0, 0.0)) * (fillCam->getViewMatrix() /** fillCam->getProjMatrix()*/);
	matrices[3] = float4x4::identity;
	context->UpdateSubresource(modelViewProjCB.Get(), 0, nullptr, matrices, 0, 0);

	static int slowingCounter = 0;
	slowingCounter++;
	if (slowingCounter % 5 == 0)
	{
		slowingCounter = 0;
		currentKey = (currentKey + 1) % nKeys;
	}

	ID3D11UnorderedAccessView* ppUnorderedAccessViews[2];
	ppUnorderedAccessViews[0] = offsetUAV.Get();
	ppUnorderedAccessViews[1] = linkUAV.Get();
	uint t[2] = { 0,0 };
	context->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, defaultDepthStencilView.Get(), 0, 2, ppUnorderedAccessViews, t);

	D3D11_VIEWPORT pViewports[1];
	pViewports->Height = fillWindowHeight;
	pViewports->Width = fillWindowWidth;
	pViewports->TopLeftX = 0;
	pViewports->TopLeftY = 0;
	pViewports->MaxDepth = 1.0;
	pViewports->MinDepth = 0.0;
	context->RSSetViewports(1, pViewports);

	animatedControlMesh->draw(context);
}

void  Game::renderAnimatedControlMeshInTPose(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
{
	//
	//uint values[4] = { 0,0,0,0 };
	//context->ClearUnorderedAccessViewUint(offsetUAV.Get(), values);

	//float4x4 matrices[4];
	//matrices[0] = float4x4::identity;
	//matrices[1] = float4x4::identity;
	//matrices[2] = float4x4::scaling(float3(animatedControlMeshScale, animatedControlMeshScale, animatedControlMeshScale))* float4x4::translation(float3(0.0, 0.0, 0.0)) * (fillCam->getViewMatrix() /** fillCam->getProjMatrix()*/);
	//matrices[3] = float4x4::identity;
	//context->UpdateSubresource(modelViewProjCB.Get(), 0, nullptr, matrices, 0, 0);

	//ID3D11UnorderedAccessView* ppUnorderedAccessViews[2];
	//ppUnorderedAccessViews[0] = offsetUAV.Get();
	//ppUnorderedAccessViews[1] = linkUAV.Get();
	//uint t[2] = { 0,0 };
	//context->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, defaultDepthStencilView.Get(), 0, 2, ppUnorderedAccessViews, t);

	//D3D11_VIEWPORT pViewports[1];
	//pViewports->Height = fillWindowHeight;
	//pViewports->Width = fillWindowWidth;
	//pViewports->TopLeftX = 0;
	//pViewports->TopLeftY = 0;
	//pViewports->MaxDepth = 1.0;
	//pViewports->MinDepth = 0.0;
	//context->RSSetViewports(1, pViewports);

	//animatedControlMesh->draw(context);

	static int slowingCounter = 0;
	slowingCounter++;
	if (slowingCounter % 5 == 0)
	{
		slowingCounter = 0;
		currentKey = (currentKey + 1) % nKeys;
	}

	DualQuaternion* boneTrafos = new DualQuaternion[nBones];
	for (int iBone = 0; iBone < nBones; iBone++) {
		boneTrafos[iBone].orientation = Egg11::Math::float4(0.0, 1.0, 0.0, 0.0);
		boneTrafos[iBone].translation = Egg11::Math::float4(0.0, 0.0, 0.0, 1.0);
		/*
		boneTrafos[iBone] = rigging[iBone];

		for (int iChain = 0; iChain < 16; iChain++) {
		auto iNode = skeleton[iChain + iBone * 16];
		if (iNode == 255) break;
		boneTrafos[iBone] = keys[
		iNode * nKeys
		+ currentKey] * boneTrafos[iBone];
		}
		*/

	}

	context->UpdateSubresource(boneBuffer.Get(), 0, nullptr, boneTrafos, 0, 0);
	delete[] boneTrafos;


	renderAnimatedControlMesh(context);

	stepAnimationKey(context);

}

void Game::renderFlatControlMesh(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
{
	uint values[4] = { 0,0,0,0 };
	context->ClearUnorderedAccessViewUint(offsetUAV.Get(), values);

	float4x4 matrices[4];
	matrices[0] = float4x4::identity;
	matrices[1] = (float4x4::scaling(float3(controlMeshScale, controlMeshScale, controlMeshScale)) * (firstPersonCam->getViewMatrix() * firstPersonCam->getProjMatrix())).invert();
	matrices[2] = (float4x4::scaling(float3(controlMeshScale, controlMeshScale, controlMeshScale)) * (firstPersonCam->getViewMatrix() * firstPersonCam->getProjMatrix()));
	//matrices[2] = float4x4::scaling(float3(animatedControlMeshScale,animatedControlMeshScale,animatedControlMeshScale)) * (fillCam->getViewMatrix() * fillCam->getProjMatrix());
	matrices[3] = fillCam->getViewDirMatrix();
	context->UpdateSubresource(modelViewProjCB.Get(), 0, nullptr, matrices, 0, 0);


	controlMeshFlat->draw(context);

}

void Game::renderFlatAnimatedControlMesh(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
{
	uint values[4] = { 0,0,0,0 };
	context->ClearUnorderedAccessViewUint(offsetUAV.Get(), values);

	float4x4 matrices[4];
	matrices[0] = float4x4::identity;
	matrices[1] = float4x4::identity;
	matrices[2] = float4x4::scaling(float3(animatedControlMeshScale, animatedControlMeshScale, animatedControlMeshScale))* float4x4::translation(float3(0.0, 0.0, 0.0)) * (firstPersonCam->getViewMatrix() * firstPersonCam->getProjMatrix());
	matrices[3] = float4x4::identity;
	context->UpdateSubresource(modelViewProjCB.Get(), 0, nullptr, matrices, 0, 0);

	animatedControlMeshFlat->draw(context);

}

void Game::fillControlParticles(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
{
	uint values[4] = { 0,0,0,0 };
	context->ClearUnorderedAccessViewUint(controlParticleUAV.Get(), values);
	context->ClearUnorderedAccessViewUint(controlParticleCounterUAV.Get(), values);

	float4x4 matrices[4];
	matrices[0] = float4x4::identity;
	matrices[1] = ((fillCam->getViewMatrix())/* * fillCam->getProjMatrix()*/).invert();
	matrices[2] = float4x4::identity;
	matrices[3] = float4x4::identity;
	context->UpdateSubresource(modelViewProjCB.Get(), 0, nullptr, matrices, 0, 0);

	controlMeshFill->getMaterial()->setCb("metaballVSTransCB", modelViewProjCB, Egg11::Mesh::ShaderStageFlag::Vertex);
	controlMeshFill->getMaterial()->setCb("metaballVSTransCB", modelViewProjCB, Egg11::Mesh::ShaderStageFlag::Pixel);

	context->PSSetShaderResources(0, 1, offsetSRV.GetAddressOf());
	context->PSSetShaderResources(1, 1, linkSRV.GetAddressOf());

	ID3D11UnorderedAccessView* ppUnorderedAccessViews[2];
	ppUnorderedAccessViews[0] = controlParticleUAV.Get();
	ppUnorderedAccessViews[1] = controlParticleCounterUAV.Get();
	uint t[2] = { 0, 0 };
	context->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, defaultDepthStencilView.Get(), 0, 2, ppUnorderedAccessViews, t);

	D3D11_VIEWPORT pViewports[1];
	pViewports->Height = fillWindowHeight;
	pViewports->Width = fillWindowWidth;
	pViewports->TopLeftX = 0;
	pViewports->TopLeftY = 0;
	pViewports->MaxDepth = 1.0;
	pViewports->MinDepth = 0.0;
	context->RSSetViewports(1, pViewports);

	controlMeshFill->draw(context);
}

void Game::setBufferForIndirectDispatch(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
{
	uint zeros[1] = { 0 };
	context->CSSetShader(static_cast<ID3D11ComputeShader*>(setIndirectDispatchBufferShader->getShader().Get()), nullptr, 0);

	context->CSSetShaderResources(0, 1, controlParticleCounterSRV.GetAddressOf());
	context->CSSetUnorderedAccessViews(0, 1, controlParticleIndirectDisptachUAV.GetAddressOf(), zeros);

	context->Dispatch(1, 1, 1);
}

void Game::setAdaptiveControlPressure(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
{
	/*
	uint zeros[1] = { 0};
	context->CSSetShader(static_cast<ID3D11ComputeShader*>(controlledFluidSimulationShader->getShader().Get()), nullptr, 0);
	context->CSSetUnorderedAccessViews(0, 1, particleUAV.GetAddressOf(), zeros);
	ID3D11ShaderResourceView* ppShaderResourceViews[2] = { controlParticleSRV.Get(), controlParticleCounterSRV.Get() };
	context->CSSetShaderResources(0, 2, ppShaderResourceViews);
	context->UpdateSubresource(controlParamsCB.Get(), 0, nullptr, &controlParams[0], 0, 0);
	uint cbindex = controlledFluidSimulationShader->getResourceIndex("controlParamsCB");
	context->CSSetConstantBuffers(cbindex, 1, controlParamsCB.GetAddressOf());
	context->Dispatch(defaultParticleCount, 1, 1);
	*/

	uint zeros[1] = { 0 };
	context->CSSetShader(static_cast<ID3D11ComputeShader*>(adaptiveControlPressureShader->getShader().Get()), nullptr, 0);

	context->CSSetUnorderedAccessViews(0, 1, controlParticleUAV.GetAddressOf(), zeros);
	context->CSSetShaderResources(0, 1, particleSRV.GetAddressOf());

	context->DispatchIndirect(controlParticleIndirectDisptachDataBuffer.Get(), 0);
	//context->Dispatch(2500, 1, 1);
}

void  Game::rigControlParticles(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
{
	uint zeros[1] = { 0 };
	context->CSSetShader(static_cast<ID3D11ComputeShader*>(rigControlParticlesShader->getShader().Get()), nullptr, 0);

	context->CSSetUnorderedAccessViews(0, 1, controlParticleUAV.GetAddressOf(), zeros);

	//uint cbindex = rigControlParticlesShader->getResourceIndex("bonePositionsCB");
	context->CSSetConstantBuffers(0, 1, bonePositionsBufferCB.GetAddressOf());

	float4x4 matrices[4];
	matrices[0] = float4x4::identity;
	matrices[1] = (float4x4::scaling(float3(animatedControlMeshScale, animatedControlMeshScale, animatedControlMeshScale))* float4x4::translation(float3(0.0, 0.0, 0.0))).invert();
	matrices[2] = float4x4::scaling(float3(animatedControlMeshScale, animatedControlMeshScale, animatedControlMeshScale))* float4x4::translation(float3(0.0, 0.0, 0.0));
	matrices[3] = float4x4::identity;
	context->UpdateSubresource(modelViewProjCB.Get(), 0, nullptr, matrices, 0, 0);
	context->CSSetConstantBuffers(1, 1, modelViewProjCB.GetAddressOf());

	context->DispatchIndirect(controlParticleIndirectDisptachDataBuffer.Get(), 0);
}

void Game::animateControlParticles(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
{
	uint zeros[1] = { 0 };
	context->CSSetShader(static_cast<ID3D11ComputeShader*>(animateControlParticlesShader->getShader().Get()), nullptr, 0);

	context->CSSetUnorderedAccessViews(0, 1, controlParticleUAV.GetAddressOf(), zeros);
	context->CSSetConstantBuffers(0, 1, boneBuffer.GetAddressOf());

	float4x4 matrices[4];
	matrices[0] = float4x4::identity;
	matrices[1] = (float4x4::scaling(float3(animatedControlMeshScale, animatedControlMeshScale, animatedControlMeshScale))* float4x4::translation(float3(0.0, 0.0, 0.0))).invert();
	matrices[2] = float4x4::scaling(float3(animatedControlMeshScale, animatedControlMeshScale, animatedControlMeshScale))* float4x4::translation(float3(0.0, 0.0, 0.0));
	matrices[3] = float4x4::identity;
	context->UpdateSubresource(modelViewProjCB.Get(), 0, nullptr, matrices, 0, 0);
	context->CSSetConstantBuffers(1, 1, modelViewProjCB.GetAddressOf());


	context->DispatchIndirect(controlParticleIndirectDisptachDataBuffer.Get(), 0);
}

void Game::stepAnimationKey(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
{
	static int slowingCounter = 0;
	slowingCounter++;
	if (slowingCounter % 5 == 0)
	{
		slowingCounter = 0;
		currentKey = (currentKey + 1) % nKeys;
	}

	DualQuaternion* boneTrafos = new DualQuaternion[nBones];
	for (int iBone = 0; iBone < nBones; iBone++) {
		boneTrafos[iBone] = rigging[iBone];
		for (int iChain = 0; iChain < 16; iChain++) {
			auto iNode = skeleton[iChain + iBone * 16];
			if (iNode == 255) break;
			boneTrafos[iBone] = keys[
				iNode * nKeys
					+ currentKey] * boneTrafos[iBone];
		}
	}

	context->UpdateSubresource(boneBuffer.Get(), 0, nullptr, boneTrafos, 0, 0);
	delete[] boneTrafos;
}

void Game::renderPBD(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context) {
	const UINT zeros[4] = { 0,0,0,0 };
	
	context->CSSetShader(static_cast<ID3D11ComputeShader*>(PBDShaderGravity->getShader().Get()), nullptr, 0);
	context->CSSetShaderResources(0, 1, controlParticleSRV.GetAddressOf());
	context->CSSetShaderResources(1, 1, controlParticleCounterSRV.GetAddressOf());
	context->CSSetUnorderedAccessViews(0, 1, controlParticleNewPosUAV.GetAddressOf(), zeros);
	context->CSSetUnorderedAccessViews(1, 1, controlParticleVelocityUAV.GetAddressOf(), zeros);
	context->Dispatch(controlParticleCount, 1, 1);
	
	clearContext(context);

	const int NITER = 50;
	for (int i = 0; i < NITER; ++i)
	{
		context->CSSetShader(static_cast<ID3D11ComputeShader*>(PBDShaderCollision->getShader().Get()), nullptr, 0);
		context->CSSetShaderResources(0, 1, controlParticleCounterSRV.GetAddressOf());
		context->CSSetUnorderedAccessViews(0, 1, controlParticleNewPosUAV.GetAddressOf(), zeros);
		context->Dispatch(controlParticleCount, 1, 1);

		context->CSSetShader(static_cast<ID3D11ComputeShader*>(PBDShaderDistance->getShader().Get()), nullptr, 0);
		context->CSSetShaderResources(0, 1, controlParticleCounterSRV.GetAddressOf());
		context->CSSetUnorderedAccessViews(0, 1, controlParticleNewPosUAV.GetAddressOf(), zeros);
		context->Dispatch(PBDGrideSize, PBDGrideSize, PBDGrideSize);
	}
	context->CSSetShader(static_cast<ID3D11ComputeShader*>(PBDShaderFinalUpdate->getShader().Get()), nullptr, 0);
	context->CSSetUnorderedAccessViews(0, 1, controlParticleUAV.GetAddressOf(), zeros);
	context->CSSetShaderResources(0, 1, controlParticleCounterSRV.GetAddressOf());
	context->CSSetShaderResources(1, 1, controlParticleNewPosSRV.GetAddressOf());	
	context->CSSetUnorderedAccessViews(1, 1, controlParticleVelocityUAV.GetAddressOf(), zeros);
	context->Dispatch(controlParticleCount, 1, 1);
}

void Game::render(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
{
	using namespace Egg11::Math;

	// Hash
	if (billboardsLoadAlgorithm == HashSimple)
	{
		// Sort
		renderSort(context);
		clearContext(context);

		// CLists
		renderInitCList(context);
		clearContext(context);

		renderNonZeroPrefix(context);
		clearContext(context);

		renderCompactCList(context);
		clearContext(context);

		renderLengthCList(context);
		clearContext(context);

		renderInitHList(context);
		clearContext(context);

		renderSortCList(context);
		clearContext(context);

		renderBeginHList(context);
		clearContext(context);

		renderLengthHList(context);
		clearContext(context);
	}

	
	//return;

	clearRenderTarget(context);

	stepAnimationKey(context);

	Egg11::Math::float4 billboardSize((1.0 / radius) *0.04, (1.0 / radius) *0.04, 0, 0);
	//Egg11::Math::float4 billboardSize(0.001, 0.001, 0, 0);

	context->UpdateSubresource(billboardSizeCB.Get(), 0, nullptr, &billboardSize, 0, 0);

	static bool first = true;


	if (controlParticlePlacement == PBD)
	{
		renderPBD(context);
	}

	//currentKey = 45;
	//if (first) stepAnimationKey(context);

	if ((first || (animtedIsActive && controlParticlePlacement == ControlParticlePlacement::Render)) && controlParticlePlacement != PBD)
		//if (first || (animtedIsActive && controlParticlePlacement == ControlParticlePlacement::Animated))
	{
		//if (controlParticlePlacement == Render)
		{
			{
				// Round1
				if (animtedIsActive)
				{
					if (controlParticlePlacement == Render)
					{
						renderAnimatedControlMesh(context);
					}
					if (controlParticlePlacement == Animated)
					{
						renderAnimatedControlMeshInTPose(context);
					}
				}
				else
				{
					renderControlMesh(context);
				}

				clearContext(context);
			}
			{
				// Round2
				fillControlParticles(context);
				clearContext(context);

				setBufferForIndirectDispatch(context);
				clearContext(context);

				if (controlParticlePlacement == Animated)
				{
					rigControlParticles(context);
					clearContext(context);
				}
			}
		}
	}
	first = false;

	if (controlParticlePlacement == Animated)
	{
		animateControlParticles(context);
		clearContext(context);
	}

	if (adapticeControlPressureIsActive)
	{
		setAdaptiveControlPressure(context);
		clearContext(context);
	}

	if (renderMode == Realistic || renderMode == Gradient)
	{

		// Billboard
		if (billboardsLoadAlgorithm == ABuffer)
		{
			renderBillboardA(context);
			clearContext(context);
		}
		else if (billboardsLoadAlgorithm == SBuffer)
		{
			renderBillboardS1(context);
			clearContext(context);

			renderPrefixSum(context);
			clearContext(context);

			// Clear count buffer
			const UINT zeros[4] = { 0,0,0,0 };
			context->ClearUnorderedAccessViewUint(countUAV.Get(), zeros);

			renderBillboardS2(context);
			clearContext(context);
		}
		else if (billboardsLoadAlgorithm == SBufferV2)
		{
			renderBillboardSV21(context);
			clearContext(context);

			renderPrefixSumV2(context);
			clearContext(context);

			// Clear count buffer
			const UINT zeros[4] = { 0,0,0,0 };
			context->ClearUnorderedAccessViewUint(countUAV.Get(), zeros);

			renderBillboardS2(context);
			clearContext(context);
		}
		else if (billboardsLoadAlgorithm == HashSimple)
		{

		}

		// Metaball
		renderMetaball(context);
		clearContext(context);

	}

	else if (renderMode == Particles)
	{
		renderParticleBillboard(context);
		//renderBalls(context);
		clearContext(context);
	}
	else if (renderMode == ControlParticles)
	{
		renderControlParticleBillboard(context);
		//renderControlBalls(context);
		clearContext(context);
	}

	// Animation
	renderAnimation(context);
	clearContext(context);

	// Sort
	//renderSort(context);
	//clearContext(context);

	//renderAnimatedControlMesh(context);
	//clearContext(context);


	if (drawFlatControlMesh)
	{
		if (animtedIsActive)
		{
			renderFlatAnimatedControlMesh(context);
		}
		else
		{
			renderFlatControlMesh(context);
		}
	}

	context->CopyStructureCount(uavCounterReadback.Get(), 0, linkUAV.Get());


	//context->CopyStructureCount(uavCounterReadback.Get(), 0, idUAV.Get());
	//context->Unmap(uavCounterReadback.Get(), 0);
	//auto b = 1;
}

void Game::animate(double dt, double t)
{
	if (!firstPersonCam) return;
	firstPersonCam->animate(dt);

}

bool Game::processMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (!firstPersonCam) return false;
	firstPersonCam->processMessage(hWnd, uMsg, wParam, lParam);

	if (uMsg == WM_KEYDOWN)
	{
		if (wParam == 'G')
		{
			renderMode = Gradient;
		}
		else if (wParam == 'R')
		{
			renderMode = Realistic;
		}
		else if (wParam == 'M')
		{
			if (shading == PhongShading)
				shading = MetalShading;
			else if (metalShading == Gold)
				metalShading = Copper;
			else if (metalShading == Copper)
				metalShading = Aluminium;
			else if (metalShading == Aluminium)
				metalShading = Gold;
		}
		else if (wParam == 'P')
		{
			shading = PhongShading;
		}
		else if (wParam == 'F')
		{
			if (metaballFunction == Simple)
				metaballFunction = Wyvill;
			else if (metaballFunction == Wyvill)
				metaballFunction = Nishimura;
			else if (metaballFunction == Nishimura)
				metaballFunction = Murakami;
			else if (metaballFunction == Murakami)
				metaballFunction = Simple;
		}
		else if (wParam == 'V')
		{
			if (waterShading == SimpleWater)
				waterShading = DeepWater;
			else if (waterShading == DeepWater)
				waterShading = SimpleWater;
		}
		else if (wParam == 'X' && radius <= 2.0)
		{
			radius += 0.1;
		}
		else if (wParam == 'Y' && radius > 0.0)
		{
			radius -= 0.1;
		}
		else if (wParam == 'O' && metaBallMinToHit < 10.0)
		{
			metaBallMinToHit += 0.1;
		}
		else if (wParam == 'I' && metaBallMinToHit > 0.1)
		{
			metaBallMinToHit -= 0.1;
		}
		else if (wParam == 'T')
		{
			if (testCount == 0) {
				radius = 0.8;
				testCount++;
			}
			else if (testCount == 1)
			{
				radius = 1.0;
				testCount++;
			}
			else if (testCount == 2)
			{
				radius = 1.3;
				testCount = 0;
			}
		}
		else if (wParam == 'U')
		{
			if (testCount2 == 0) {
				metaBallMinToHit = 0.2;
				testCount2++;
			}
			else if (testCount2 == 1)
			{
				metaBallMinToHit = 0.9;
				testCount2++;
			}
			else if (testCount2 == 2)
			{
				metaBallMinToHit = 1.5;
				testCount2 = 0;
			}
		}
		else if (wParam == 'H')
		{
			if (binaryStepCount == 4)
				binaryStepCount = 0;
			else
				binaryStepCount += 2;
		}
		else if (wParam == 'J')
		{
			if (maxRecursion == 3)
				maxRecursion = 1;
			else
				maxRecursion += 1;
		}
		else if (wParam == 'K')
		{
			if (marchCount == 40)
				marchCount = 25;
			else
				marchCount = 40;
		}
		else if (wParam == '0')
		{
			if (billboardsLoadAlgorithm == Normal)
			{
				billboardsLoadAlgorithm = ABuffer;
			}
			else if (billboardsLoadAlgorithm == ABuffer)
			{
				billboardsLoadAlgorithm = SBuffer;
			}
			else if (billboardsLoadAlgorithm == SBuffer)
			{
				billboardsLoadAlgorithm = HashSimple;
			}
			else if (billboardsLoadAlgorithm == HashSimple)
			{
				billboardsLoadAlgorithm = Normal;
			}
		}
		else if (wParam == '1')
		{
			billboardsLoadAlgorithm = Normal;
			if (renderMode != Particles)
			{
				renderMode = Particles;
				debugType = 0;
			}
			else
			{
				debugType = (debugType + 1) % maxDebugType;
			}
		}
		else if (wParam == '2')
		{
			billboardsLoadAlgorithm = Normal;
			renderMode = ControlParticles;
		}
		else if (wParam == '3')
		{
			flowControl = (FlowControl)((flowControl + 1) % 2);
		}
		else if (wParam == '4')
		{
			if (controlParams[0] < 0.5)
			{
				controlParams[0] = 1.0;
			}
			else
			{
				controlParams[0] = 0.0;
			}
		}
		else if (wParam == '5')
		{
			if (controlParams[1] < 0.5)
			{
				controlParams[1] = 1.0;
			}
			else
			{
				controlParams[1] = 0.0;
			}
		}
		else if (wParam == '6')
		{
			if (controlParams[2] < 0.5)
			{
				controlParams[2] = 1.0;
			}
			else
			{
				controlParams[2] = 0.0;
			}
		}
		else if (wParam == '7')
		{
			if (controlParams[3] < 0.5)
			{
				controlParams[3] = 1.0;
			}
			else
			{
				controlParams[3] = 0.0;
			}
		}
		else if (wParam == '8')
		{
			//controlParams[7] -= 0.1;
			//std::cout << controlParams[7] << std::endl;

			currentKey = 0;
		}
		else if (wParam == '9')
		{
			//controlParams[7] += 0.1;
			//std::cout << controlParams[7] << std::endl;
			if (drawFlatControlMesh)
			{
				drawFlatControlMesh = false;
			}
			else
			{
				drawFlatControlMesh = true;
			}
		}
	}

	return false;
}
