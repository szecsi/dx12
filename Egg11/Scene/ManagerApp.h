#pragma once
#include "Scene/Entity.h"
#include "Scene/RenderParameters.h"
#include "Scene/ControlParameters.h"
#include "Mesh/Indexed.h"
#include "Mesh/Multi.h"
#include "Mesh/InputBinder.h"
#include "Cam/Base.h"
#include "App/App.h"
#include <assimp/importer.hpp>      // C++ importer interface
#include <assimp/scene.h>       // Output data structure
#include <assimp/postProcess.h> // Post processing flags

namespace Egg11 { namespace Scene
{
	/// Application class with scene management
	GG_SUBCLASS(ManagerApp, Egg11::App)
	protected:
		/// Mesh binder
		Egg11::Mesh::InputBinder::P inputBinder;

		/// Called from loadMultiMesh(). The contents of the file are imported as aiMeshes. From every aiMesh, a Mesh::Indexed is created.
		/// Then this function is called to create all necessary ShadedMesh instances, and add them to the Mesh::Flip.
		/// This can be overridden to support multiple Mien-s.
		virtual void addDefaultShadedMeshes(Mesh::Flip::P flipMesh, Mesh::Indexed::P indexedMesh, aiMaterial* assMaterial);
		/// Imports a file with AssImp and creates a Mesh::Multi. Uses addDefaultShadedMeshes() to create the ShadedMesh instances.
		Mesh::Multi::P loadMultiMesh(const std::string& filename, unsigned int flags, const std::string& alias = "");

		/// Master structure containing everything that should be passed to Entity::render().
		RenderParameters renderParameters;
		/// Master structure containing everything that should be passed to Entity::control().
		ControlParameters controlParameters;

		typedef std::map<std::string, Mesh::Mien> MienDirectory;
		/// Directory containing all rendering Miens used.  Addressed by mien name (e.g. "basic").
		MienDirectory miens;
		std::map<std::string, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> > srvs;
		std::map<std::string, Egg11::Mesh::Indexed::P > indexedMeshes;

		std::vector<Entity::P> entities;
		std::vector<Cam::Base::P> cameras;
		/// Iterator to current camera in cameras. Must be updated when cameras is changed.
		uint currentCamera;

		/// Returns the SRV to a texture resource, creating it from file if not yet loaded.
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> loadSrv(const std::string& filename, const std::string& alias = "");
		/// Returns a Mesh::Indexed, creating it from file if not yet loaded.
		Mesh::Indexed::P loadIndexedMesh(const std::string& filename, const std::string& alias = "");
		/// Returns a Mien, creating it if it does not exist yet.
		Mesh::Mien getMien(const std::string& mienName);

	public:
		ManagerApp(Microsoft::WRL::ComPtr<ID3D11Device> device): Egg11::App(device){currentCamera=0;}
		virtual HRESULT createResources();
		virtual HRESULT releaseResources();

		virtual void animate(double dt, double t);
		virtual bool processMessage( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
		virtual void render(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);

	GG_ENDCLASS

}}