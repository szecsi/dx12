#pragma once

#include "../Common.h"
#include "Entity.h"
#include "Egg/Mesh/Geometry.h"
#include "Egg/Mesh/Multi.h"
#include "Egg/Mesh/Material.h"
#include "Egg/Cam/FirstPerson.h"
#include "Egg/Importer.h"
#include "Egg/SimpleApp.h"
#include "Egg/ConstantBuffer.hpp"
#include "Egg/Shader.h"
#include "ConstantBufferTypes.h"

#include <map>

namespace Egg {
	namespace Scene {
		/// Application class with scene management
		GG_SUBCLASS( ManagerApp, SimpleApp )
		protected:

			com_ptr<ID3DBlob> defaultVertexShader;
			com_ptr<ID3DBlob> defaultPixelShader;
			com_ptr<ID3D12RootSignature> defaultRootSig;
			std::string envTexturePath;

			/// Called from loadMultiMesh(). The contents of the file are imported as aiMeshes. From every aiMesh, a Mesh::Indexed is created.
			/// Then this function is called to create all necessary ShadedMesh instances, and add them to the Mesh::Flip.
			/// This can be overridden to support multiple Mien-s.
			virtual void AddDefaultShadedMeshes(
				Egg::Mesh::Flip::P flipMesh,
				Egg::Mesh::IndexedGeometry::P indexedGeometry,
				std::string texturePath
				//aiMaterial* assMaterial
			) {
				//aiString texturePath;
				//assMaterial->GetTexture(aiTextureType::aiTextureType_DIFFUSE, 0, &texturePath);

				Egg::Texture2D tex = LoadTexture2D(texturePath);
				tex.CreateSRV(device.Get(), srvHeap.Get(), srvCount);

				Egg::TextureCube env = LoadTextureCube(envTexturePath);
				env.CreateSRV(device.Get(), srvHeap.Get(), srvCount+1);

				Mesh::Material::P material = Egg::Mesh::Material::Create();
				material->SetRootSignature(defaultRootSig);
				material->SetVertexShader(defaultVertexShader);
				material->SetPixelShader(defaultPixelShader);
				material->SetDepthStencilState(CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT));
				material->SetDSVFormat(DXGI_FORMAT_D32_FLOAT);
				material->SetSrvHeap(2, srvHeap,
					srvCount * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
				material->SetConstantBuffer(perObjectCb, sizeof(Egg::Scene::PerObjectData));
				material->SetConstantBuffer(perFrameCb);
				srvCount+=2;

				auto shadedMesh = Egg::Mesh::Shaded::Create(psoManager.get(), material, indexedGeometry);
				flipMesh->Add(0, shadedMesh);
			}

			/// Imports a file with AssImp and creates a Mesh::Multi. Uses addDefaultShadedMeshes() to create the ShadedMesh instances.
			Mesh::Multi::P LoadMultiMesh(const std::string& filename, unsigned int flags = -1, const std::string& alias = "");

			std::map<std::string, Egg::Mesh::IndexedGeometry::P> indexedGeometries;
			std::vector<Egg::Scene::Entity::P> entities;
			std::vector<Egg::Cam::Base::P> cameras;
			unsigned int currentCameraIndex;

			/// Returns the SRV to a texture resource, creating it from file if not yet loaded.
			Egg::Texture2D LoadTexture2D(const std::string& filename) {
				std::string path = "../Media/" + filename;
				if (filename.empty())
					path = std::string("../Media/uvGrid.png");

				auto i = textures.find(path);
				if (i != textures.end())
					return i->second;

				Egg::Texture2D tex = Importer::ImportTexture2D(device.Get(), path);
				textures[path] = tex;
				return tex;
			}
			Egg::TextureCube LoadTextureCube(const std::string& filename) {
				std::string path = "../Media/" + filename;

				auto i = textureCubes.find(path);
				if (i != textureCubes.end())
					return i->second;

				Egg::TextureCube tex = Importer::ImportTextureCube(device.Get(), path);
				textureCubes[path] = tex;
				return tex;
			}
			/// Returns a Mesh::Indexed, creating it from file if not yet loaded.
			Mesh::IndexedGeometry::P loadIndexedGeometry(const std::string& filename, const std::string& alias = "");

			Egg::ConstantBuffer<PerObjectCb> perObjectCb;
			Egg::ConstantBuffer<PerFrameCb> perFrameCb;
			com_ptr<ID3D12DescriptorHeap> srvHeap;
			unsigned int srvCount;
			std::map<std::string, Egg::Texture2D> textures;
			std::map<std::string, Egg::TextureCube> textureCubes;

			ManagerApp() :SimpleApp(), srvCount{ 0 }, envTexturePath{"cloudyNoon.dds"}{}
		public:
			virtual void Update(float dt, float T) override {
				using namespace Egg::Math;
				unsigned int iEntity = 0;
				for (auto entity : entities) {
					entity->Update(dt, T, perObjectCb->objects[iEntity]);
					iEntity++;
				}
				perObjectCb.Upload();

				auto camera = cameras[currentCameraIndex];
				camera->Animate(dt);

				perFrameCb->viewProjTransform =
					camera->GetViewMatrix() *
					camera->GetProjMatrix();
				perFrameCb->rayDirTransform = camera->GetRayDirMatrix();
				perFrameCb->cameraPos = Float4(camera->GetEyePosition(), 1);
				perFrameCb->lightPos = Float4(0, 1, 0, 0);
				perFrameCb->lightPowerDensity = Float4(1, 1, 1, 0);
				perFrameCb->billboardSize = Float4(50.1, 50.1, 0, 0) * camera->GetProjMatrix();
				perFrameCb.Upload();
			}
			virtual void LoadAssets() override {
				perObjectCb.CreateResources(device.Get());
				perFrameCb.CreateResources(device.Get());

				defaultVertexShader = Egg::Shader::LoadCso("Shaders/trafoVS.cso");
				defaultPixelShader = Egg::Shader::LoadCso("Shaders/MaxBlinnPS.cso");
				defaultRootSig = Egg::Shader::LoadRootSignature(device.Get(), defaultVertexShader.Get());

				cameras.push_back( Egg::Cam::FirstPerson::Create() );
			}

			virtual void UploadResources() {
				for (auto& tex : textures) {
					tex.second.UploadResource(commandList.Get());
				}
				for (auto& tex : textureCubes) {
					tex.second.UploadResource(commandList.Get());
				}
			}

			virtual void ReleaseUploadResources() {
				for (auto& tex : textures) {
					tex.second.ReleaseUploadResources();
				}
				for (auto& tex : textureCubes) {
					tex.second.ReleaseUploadResources();
				}
			}

			virtual void CreateResources() override {
				Egg::SimpleApp::CreateResources();
				D3D12_DESCRIPTOR_HEAP_DESC dhd;

				dhd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
				dhd.NodeMask = 0;
				dhd.NumDescriptors = 1024;
				dhd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

				DX_API("Failed to create descriptor heap for texture")
					device->CreateDescriptorHeap(&dhd, IID_PPV_ARGS(srvHeap.GetAddressOf()));

			}

			virtual void ReleaseResources() override {
				srvHeap.Reset();

				Egg::SimpleApp::ReleaseResources();
			}

			void ProcessMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override
			{
				if (cameras.empty())
					return;
				cameras[currentCameraIndex]->ProcessMessage(hWnd, uMsg, wParam, lParam);
			}

			void CreateSwapChainResources() override {
				__super::CreateSwapChainResources();
				if (cameras.empty())
					return;
				cameras[currentCameraIndex]->SetAspect(aspectRatio);
			}

			virtual void PopulateCommandList() override {
				for (int i = 0; i < entities.size(); i++) {
					entities[i]->Draw(commandList.Get(), 0, i);
				}
			}

			virtual void AddEntity(Egg::Scene::Entity::P entity) {
				entities.push_back(entity);
			}

		};
	}
}