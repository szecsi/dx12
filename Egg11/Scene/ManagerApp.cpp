#include "stdafx.h"
#include "Scene/ManagerApp.h"
#include "App/UtfConverter.h"
#include "Mesh/Indexed.h"
#include "Mesh/Importer.h"
#include "Cam/FirstPerson.h"
#include "ThrowOnFail.h"
#include "App/stdConversions.h"
#include <DirectXTex/DirectXTex.h>
#include "DDSTextureLoader.h"

using namespace Egg11;
using namespace Microsoft::WRL;

HRESULT Scene::ManagerApp::createResources()
{
	App::createResources();
	inputBinder = Egg11::Mesh::InputBinder::create(device);

	renderParameters.mien = getMien("basic");

	D3D11_BUFFER_DESC constantBufferDesc;
	constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	constantBufferDesc.ByteWidth = sizeof(Egg11::Math::float4x4) * 4;
	constantBufferDesc.CPUAccessFlags = 0;
	constantBufferDesc.MiscFlags = 0;
	constantBufferDesc.StructureByteStride = 0;
	constantBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	Egg11::ThrowOnFail("Failed to create per object constant buffer.", __FILE__, __LINE__) ^
		device->CreateBuffer(&constantBufferDesc, nullptr, renderParameters.perObjectConstantBuffer.GetAddressOf());

	constantBufferDesc.ByteWidth = sizeof(Egg11::Math::float4) * 1;
	Egg11::ThrowOnFail("Failed to create per frame constant buffer.", __FILE__, __LINE__) ^
		device->CreateBuffer(&constantBufferDesc, nullptr, renderParameters.perFrameConstantBuffer.GetAddressOf());

	cameras.push_back( Egg11::Cam::FirstPerson::create() );


	return S_OK;
}

HRESULT Scene::ManagerApp::releaseResources()
{
	inputBinder.reset();
	srvs.clear();
	indexedMeshes.clear();
	cameras.clear();
	entities.clear();
	return App::releaseResources();
}

void Scene::ManagerApp::render(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
{
	renderParameters.context = context;
	renderParameters.camera = cameras.at(currentCamera);
	for (auto& entity : entities) {
		entity->render(renderParameters);
	}
	App::render(context);
}

void Scene::ManagerApp::animate(double dt, double t)
{
	App::animate(dt, t);
	cameras.at(currentCamera)->animate(dt);

	for (auto& entity : entities) {
		entity->animate(dt, t);
	}
}

bool Scene::ManagerApp::processMessage( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	controlParameters.processMessage(hWnd, uMsg, wParam, lParam);
	cameras.at(currentCamera)->processMessage(hWnd, uMsg, wParam, lParam);
	if(uMsg == WM_KEYDOWN && wParam == 'B')
	{
		currentCamera++;
		if(currentCamera == cameras.size())
			currentCamera = 0;
	}
	return App::processMessage(hWnd, uMsg, wParam, lParam);
}

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Scene::ManagerApp::loadSrv(const std::string& filename, const std::string& alias)
{
	std::string filepath = App::getSystemEnvironment().resolveMediaPath(filename);
	if(filename == "")
		filepath = App::getSystemEnvironment().resolveMediaPath("uvGrid.jpg");
		
	auto i = srvs.find( filepath );
	if(i != srvs.end())
		return i->second;

	std::wstring file = Egg11::UtfConverter::utf8to16(filepath);

	DirectX::ScratchImage si;
	DirectX::ScratchImage simipmap;
	DirectX::TexMetadata metadata;
	Egg11::ThrowOnFail("Failed to load texture.", __FILE__, __LINE__) ^
		DirectX::LoadFromWICFile(file.c_str(), 0, &metadata, si);

	Egg11::ThrowOnFail("Failed generating mipmaps.", __FILE__, __LINE__) ^
		DirectX::GenerateMipMaps(si.GetImage(0, 0, 0), 1, metadata, (DWORD)DirectX::TEX_FILTER_DEFAULT, 0, simipmap);

	ComPtr<ID3D11Resource> resource;

	Egg11::ThrowOnFail("Could not create 2D texture.", __FILE__, __LINE__) ^
		DirectX::CreateTextureEx(
			device.Get(),
			simipmap.GetImages(), simipmap.GetImageCount(), metadata,
			D3D11_USAGE_DEFAULT,
			D3D11_BIND_SHADER_RESOURCE,
			0,
			0,
			false, resource.GetAddressOf());

	ComPtr<ID3D11Texture2D> texture;
	resource.As<ID3D11Texture2D>(&texture);

	D3D11_TEXTURE2D_DESC tdesc;
	texture->GetDesc(&tdesc);

	D3D11_SHADER_RESOURCE_VIEW_DESC desc;
	desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	desc.Format = tdesc.Format;
	desc.Texture2D.MipLevels = tdesc.MipLevels;
	desc.Texture2D.MostDetailedMip = 0;

	ComPtr<ID3D11ShaderResourceView> tsrv;
	ThrowOnFail((std::string("Could not load texture file: ") + filepath).c_str(), __FILE__, __LINE__) = 
		device->CreateShaderResourceView(texture.Get(), &desc, tsrv.GetAddressOf());

	srvs[filepath] = tsrv;
	if(!alias.empty())
		srvs[alias] = tsrv;
	return tsrv;
}

Mesh::Indexed::P Scene::ManagerApp::loadIndexedMesh(const std::string& filename, const std::string& alias)
{
	std::string filepath = App::getSystemEnvironment().resolveMediaPath(filename);
	auto i = indexedMeshes.find( filepath );
	if(i != indexedMeshes.end())
		return i->second;

	Assimp::Importer importer;
	const aiScene* assScene = importer.ReadFile( filepath, 0);
  
	// if the import failed
	if( !assScene || !assScene->HasMeshes() || assScene->mNumMeshes == 0) 
	{
		return Mesh::Indexed::P();
	}
	Egg11::Mesh::Indexed::P indexedMesh = Egg11::Mesh::Importer::fromAiMesh(device, assScene->mMeshes[0]);
	indexedMeshes[filepath] = indexedMesh;
	if(!alias.empty())
		indexedMeshes[alias] = indexedMesh;
	return indexedMesh;
}

Mesh::Mien Scene::ManagerApp::getMien(const std::string& mienName)
{
	MienDirectory::iterator i = miens.find(mienName);
	if(i != miens.end())
		return i->second;
	Mesh::Mien mien;
	miens[mienName] = mien;
	return mien;
}

Mesh::Multi::P Scene::ManagerApp::loadMultiMesh(const std::string& filename, unsigned int flags, const std::string& alias)
{
	std::string filepath = App::getSystemEnvironment().resolveMediaPath(filename);

	Assimp::Importer importer;

	const aiScene* assScene = importer.ReadFile( filepath, flags);
  
	// if the import failed
	if( !assScene || !assScene->HasMeshes() || assScene->mNumMeshes == 0) 
	{
		std::string error = importer.GetErrorString();
		throw HrException(E_FAIL, error.c_str(), __FILE__, __LINE__);
		return Mesh::Multi::P();
	}

	Mesh::Multi::P multiMesh = Mesh::Multi::create();

	for(int i=0; i<assScene->mNumMeshes; i++)
	{
		Mesh::Indexed::P indexedMesh = Mesh::Importer::fromAiMesh(device, assScene->mMeshes[i]);
		if(!alias.empty())
			indexedMeshes[alias + "[" + toString(i) + "]"] = indexedMesh;

		Mesh::Flip::P flipMesh = Mesh::Flip::create();
		addDefaultShadedMeshes(flipMesh, indexedMesh,
			assScene->mMaterials[assScene->mMeshes[i]->mMaterialIndex]);

		multiMesh->add(flipMesh);
	}
	return multiMesh;
}

void Scene::ManagerApp::addDefaultShadedMeshes(Mesh::Flip::P flipMesh, Mesh::Indexed::P indexedMesh, aiMaterial* assMaterial)
{
		aiString texturePath;
		assMaterial->GetTexture(aiTextureType::aiTextureType_DIFFUSE, 0, &texturePath);

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv = loadSrv(texturePath.data);

		ComPtr<ID3DBlob> vertexShaderByteCode = loadShaderCode("vsTrafo.cso");
		Egg11::Mesh::Shader::P vertexShader = Egg11::Mesh::Shader::create("vsTrafo.cso", device, vertexShaderByteCode);

		ComPtr<ID3DBlob> pixelShaderByteCode = loadShaderCode("psIdle.cso");
		Egg11::Mesh::Shader::P pixelShader = Egg11::Mesh::Shader::create("psIdle.cso", device, pixelShaderByteCode);

		Mesh::Material::P material = Mesh::Material::create();
		material->setShader(Egg11::Mesh::ShaderStageFlag::Vertex, vertexShader);
		material->setShader(Egg11::Mesh::ShaderStageFlag::Pixel, pixelShader);

		// TODO Set shaderStage
		//material->setShaderResource("kd", srv);
		//material->setCb("perObject", renderParameters.perObjectConstantBuffer);
		//material->setCb("perFrame", renderParameters.perFrameConstantBuffer);

		ComPtr<ID3D11InputLayout> inputLayout = inputBinder->getCompatibleInputLayout(vertexShaderByteCode, indexedMesh);
		Mesh::Shaded::P shadedMesh = Egg11::Mesh::Shaded::create(indexedMesh, material, inputLayout);

		flipMesh->add( getMien("basic"), shadedMesh);
}
