#pragma once
#include "../Common.h"
#include "Egg/Scene/ManagerApp.h"
#include "Egg/Scene/StaticEntity.h"
#include "Egg/Mesh/Material.h"
#include "Egg/Mesh/Multi.h"
#include "Shader.h"

#include "Egg/Script/luabindGetPointer.h"
extern "C"
{
#include "Lua/lua.h"
#include "Lua/lualib.h"
}
#include "LuaBind/luabind/luabind.hpp"
#include "LuaBind/luabind/adopt_policy.hpp"

namespace Egg {
	namespace Script {
		/// Application class with scene management
		GG_SUBCLASS(ScriptedApp, Egg::Scene::ManagerApp)
protected:
	lua_State* luaState;

	void ExitWithErrorMessage(std::exception exception);

public:
	Egg::Script::Shader::P CreateShader(luabind::object nil, luabind::object attributes);
	Egg::Mesh::Geometry::P CreateIndexedGeometry(luabind::object nil, luabind::object attributes);
	Egg::Mesh::Material::P CreateMaterial(luabind::object nil, luabind::object attributes, luabind::object initalizer);
	void AddTexture2DToMaterial(Egg::Mesh::Material::P material, luabind::object attributes);
	void AddTextureCubeToMaterial(Egg::Mesh::Material::P material, luabind::object attributes);

	Egg::Mesh::Multi::P CreateMultiMesh(luabind::object nil, luabind::object attributes, luabind::object initalizer);
	void AddFlipMeshToMultiMesh(Egg::Mesh::Multi::P multiMesh, luabind::object attributes, luabind::object initalizer);
	void AddShadedMeshToFlipMesh(Egg::Mesh::Flip::P flipMesh, luabind::object attributes);

	Egg::Mesh::Multi::P CreateMultiMeshFromFile(luabind::object nil, luabind::object attributes);

	Egg::Scene::StaticEntity::P CreateStaticEntity(luabind::object nil, luabind::object attributes);
	Egg::Cam::FirstPerson::P CreateFirstPersonCam(luabind::object nil, luabind::object attributes);
	//void addFixedCam(luabind::object nil, luabind::object attributes);

	void RunScript(const std::string& luaFilename);

	virtual void LoadAssets() override;

	virtual void ReleaseResources() override;
	GG_ENDCLASS
	}
}
