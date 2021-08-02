#include "ScriptedApp.h"
#include "LuaTable.h"
#include "Egg/Scene/Entity.h"
#include "Egg/Scene/FixedRigidBody.h"
#include "Egg/Cam/FirstPerson.h"
//#include "Egg/Cam/Fixed.h"
#include "AiEnumReflections.h"
#include <memory>
#include <assimp/importer.hpp>      // C++ importer interface
#include <assimp/scene.h>       // Output data structure
#include <assimp/postProcess.h> // Post processing flags

using namespace Egg;

int luabindErrorHandler(lua_State* L)
{
	// log the error message
	luabind::object msg(luabind::from_stack(L, -1));
	std::ostringstream str;
	str << "lua> run-time error: " << msg;

	MessageBoxA(NULL, str.str().c_str(), "Lua error!", MB_OK);

	return 1;
}

void Script::ScriptedApp::LoadAssets() {
	__super::LoadAssets();

	using namespace luabind;

	luaState = lua_open();

	open(luaState);
	luaL_openlibs(luaState);

	module(luaState)
	[
		class_<Mesh::Geometry>("CGeometry"),
		class_<Mesh::Flip>("CFlip"),
		class_<Mesh::Multi>("CMulti")
			.def("getGeometry", &Mesh::Multi::GetGeometry),
		class_<Mesh::Material>("CMaterial"),
		class_< Egg::Script::Shader >("CShader"),
		class_<Scene::Entity>("CEntity"),
		class_<Cam::FirstPerson>("CFPSC"),
		class_<Cam::Fixed>("CFixedCam"),

		class_<Script::ScriptedApp>("ScriptedApp")
		.def("IndexedGeometry", &Script::ScriptedApp::CreateIndexedGeometry)
		.def("IndexedGeometryWithTangentSpace", &Script::ScriptedApp::CreateIndexedGeometryWithTangentSpace)
		.def("Shader", &Script::ScriptedApp::CreateShader)
		.def("Material", &Script::ScriptedApp::CreateMaterial)
		.def("setTexture2D", &Script::ScriptedApp::AddTexture2DToMaterial)
		.def("setTextureCube", &Script::ScriptedApp::AddTextureCubeToMaterial)
		.def("MultiMesh", &Script::ScriptedApp::CreateMultiMesh)
		.def("FlipMesh", &Script::ScriptedApp::AddFlipMeshToMultiMesh)
		.def("ShadedMesh", &Script::ScriptedApp::AddShadedMeshToFlipMesh)
		.def("MultiMeshFromFile", &Script::ScriptedApp::CreateMultiMeshFromFile)
		.def("StaticEntity", &Script::ScriptedApp::CreateStaticEntity)
		.def("FirstPersonCam", &Script::ScriptedApp::CreateFirstPersonCam)
		.def("FixedCam", &Script::ScriptedApp::CreateFixedCam)
		];

	AiEnumReflections::initialize();

	int s = luaL_dostring(luaState, "O = nil; _ = nil; function setEgg(egg) O = egg end");
	if (s != 0) { std::string errs = lua_tostring(luaState, -1); MessageBoxA(NULL, errs.c_str(), "Lua error!", MB_OK); }
	call_function<Script::ScriptedApp*>(luaState, "setEgg", this);

	set_pcall_callback(luabindErrorHandler);
}


void Script::ScriptedApp::ReleaseResources()
{
	__super::ReleaseResources();
	lua_close(luaState);
}


void Script::ScriptedApp::ExitWithErrorMessage(std::exception exception)
{
	std::stringstream errorMessage;
	MessageBoxA(NULL, exception.what(), "Error!", MB_OK);
	exit(-1);
}

Egg::Script::Shader::P Script::ScriptedApp::CreateShader(luabind::object nil, luabind::object attributes)
{
	LuaTable attributeTable(attributes, "Shader");
	try
	{
		std::string fileName = attributeTable.getString("file");
		auto shader = Egg::Script::Shader::Create();
		shader->byteCode = Egg::Shader::LoadCso(fileName);
		shader->rootSig = Egg::Shader::LoadRootSignature(device.Get(), fileName);
		return shader;
	}
	catch (std::exception exception) { ExitWithErrorMessage(exception); }

}


Egg::Mesh::Geometry::P Script::ScriptedApp::CreateIndexedGeometry(luabind::object nil, luabind::object attributes)
{
	LuaTable attributeTable(attributes, "IndexedGeometry");
	try
	{
		std::string fileName = attributeTable.getString("file");
		Egg::Mesh::IndexedGeometry::P indexedGeometry = std::dynamic_pointer_cast<Egg::Mesh::IndexedGeometry>(
			Egg::Importer::ImportSimpleObj(device.Get(), fileName));
		return indexedGeometry;
	}
	catch (std::exception exception) { ExitWithErrorMessage(exception); }
}

Egg::Mesh::Geometry::P Script::ScriptedApp::CreateIndexedGeometryWithTangentSpace(luabind::object nil, luabind::object attributes)
{
	LuaTable attributeTable(attributes, "IndexedGeometryWithTangentSpace");
	try
	{
		std::string fileName = attributeTable.getString("file");
		Egg::Mesh::IndexedGeometry::P indexedGeometry = std::dynamic_pointer_cast<Egg::Mesh::IndexedGeometry>(
			Egg::Importer::ImportWithTangentSpace(device.Get(), fileName));
		return indexedGeometry;
	}
	catch (std::exception exception) { ExitWithErrorMessage(exception); }
}

Egg::Mesh::Material::P Script::ScriptedApp::CreateMaterial(luabind::object nil, luabind::object attributes, luabind::object initializer)
{
	LuaTable attributeTable(attributes, "MeshMaterial");
	try
	{
		bool useDepthTest = attributeTable.getBool("useDepthTest", true);
		bool usePerObjectData = attributeTable.getBool("usePerObjectData", true);
		bool wireframe = attributeTable.getBool("wireframe", false);
		Egg::Mesh::Material::P material = Egg::Mesh::Material::Create();
		unsigned int srvStartIndex = srvCount;
		unsigned int srvDescriptorTableRootParameterIndex = attributeTable.getInt("rootParameterIndex", 2);
		auto vs = attributeTable.
			get<Egg::Script::Shader>("vs");
		auto gs = attributeTable.get<Egg::Script::Shader>("gs", Egg::Script::Shader::P());
		auto hs = attributeTable.get<Egg::Script::Shader>("hs", Egg::Script::Shader::P());
		auto ds = attributeTable.get<Egg::Script::Shader>("ds", Egg::Script::Shader::P());
		auto ps = attributeTable.get<Egg::Script::Shader>("ps");
		initializer(material);
		material->SetSrvHeap(srvDescriptorTableRootParameterIndex, srvHeap, srvStartIndex * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

		if (wireframe) {
			D3D12_RASTERIZER_DESC rsd;
			rsd.FillMode = D3D12_FILL_MODE_WIREFRAME;
			rsd.CullMode = D3D12_CULL_MODE_NONE;
			rsd.FrontCounterClockwise = true;
			rsd.DepthBias = 0;
			rsd.SlopeScaledDepthBias = 0.0;
			rsd.DepthBiasClamp = 0.0;
			rsd.DepthClipEnable = true;
			rsd.MultisampleEnable = false;
			rsd.AntialiasedLineEnable = false;
			rsd.ForcedSampleCount = 0;
			rsd.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
			material->SetRasterizerState(rsd);
		}
		material->SetRootSignature(vs->rootSig);
		material->SetVertexShader(vs->byteCode);
		if(gs)
			material->SetGeometryShader(gs->byteCode);
		if (hs)
			material->SetHullShader(hs->byteCode);
		if (ds)
			material->SetDomainShader(ds->byteCode);
		material->SetPixelShader(ps->byteCode);

		if(useDepthTest)
			material->SetDepthStencilState(CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT));
		else {
			auto nwdss = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			nwdss.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
			material->SetDepthStencilState(nwdss);
		}

		material->SetDSVFormat(DXGI_FORMAT_D32_FLOAT);
		if(usePerObjectData)
			material->SetConstantBuffer(perObjectCb, sizeof(Egg::Scene::PerObjectData));
		material->SetConstantBuffer(perFrameCb);

		return material;
	}
	catch (std::exception exception) { ExitWithErrorMessage(exception); }
}

void Script::ScriptedApp::AddTexture2DToMaterial(Egg::Mesh::Material::P material, luabind::object attributes)
{
	LuaTable attributeTable(attributes, "setTexture2D");
	try
	{
		std::string fileName = attributeTable.getString("file");
		auto tex = LoadTexture2D(fileName);
		tex.CreateSRV(device.Get(), srvHeap.Get(), srvCount);
		srvCount++;
	}
	catch (std::exception exception) { ExitWithErrorMessage(exception); }
}

void Script::ScriptedApp::AddTextureCubeToMaterial(Egg::Mesh::Material::P material, luabind::object attributes)
{
	LuaTable attributeTable(attributes, "setTextureCube");
	try
	{
		std::string fileName = attributeTable.getString("file");
		auto tex = LoadTextureCube(fileName);
		tex.CreateSRV(device.Get(), srvHeap.Get(), srvCount);
		srvCount++;
	}
	catch (std::exception exception) { ExitWithErrorMessage(exception); }
}

Egg::Mesh::Multi::P Script::ScriptedApp::CreateMultiMesh(luabind::object nil, luabind::object attributes, luabind::object initializer)
{
	LuaTable attributeTable(attributes, "MultiMesh");
	try
	{
		Egg::Mesh::Multi::P multiMesh = Egg::Mesh::Multi::Create();
		initializer(multiMesh);
		return multiMesh;
	}
	catch (std::exception exception) { ExitWithErrorMessage(exception); }
}

void Script::ScriptedApp::AddFlipMeshToMultiMesh(Egg::Mesh::Multi::P multiMesh, luabind::object attributes, luabind::object initializer)
{
	LuaTable attributeTable(attributes, "FlipMesh");
	try
	{
		Egg::Mesh::Flip::P flipMesh = Egg::Mesh::Flip::Create();
		initializer(flipMesh);
		multiMesh->Add(flipMesh);
	}
	catch (std::exception exception) { ExitWithErrorMessage(exception); }
}

void Script::ScriptedApp::AddShadedMeshToFlipMesh(Egg::Mesh::Flip::P flipMesh, luabind::object attributes)
{
	LuaTable attributeTable(attributes, "ShadedMesh");
	try
	{
		unsigned int mien = attributeTable.getInt("mien", 0);
		auto geometry = attributeTable.
			get<Mesh::Geometry>("geometry");
		auto material= attributeTable.get<Mesh::Material>("material");
		Egg::Mesh::Shaded::P shadedMesh = Mesh::Shaded::Create(psoManager, material, geometry);
		flipMesh->Add(mien, shadedMesh);
	}
	catch (std::exception exception) { ExitWithErrorMessage(exception); }
}

Egg::Mesh::Multi::P Script::ScriptedApp::CreateMultiMeshFromFile(luabind::object nil, luabind::object attributes)
{
	LuaTable attributeTable(attributes, "MultiMeshFromFile");
	try
	{
		std::string filename = attributeTable.getString("file");
		std::string topo = attributeTable.getString("topology", "triangle_list");
		unsigned int flags = attributeTable.getEnumCombination<aiPostProcessSteps, unsigned int>("flags", 
			aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_GenUVCoords);
		Egg::Mesh::Multi::P multi = LoadMultiMesh(filename, flags);
		if (topo == "patch") {
			multi->SetTopology(D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
		}
		if (topo == "patch4") {
			multi->SetTopology(D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);
		}
		return multi;
	}
	catch (std::exception exception) { ExitWithErrorMessage(exception); }
}

Egg::Scene::Entity::P Script::ScriptedApp::CreateStaticEntity(luabind::object nil, luabind::object attributes)
{
	LuaTable attributeTable(attributes, "StaticEntity");
	try
	{
		auto multiMesh = attributeTable.get<Egg::Mesh::Multi>("multiMesh");
		using namespace Egg::Math;
		Float3 position = attributeTable.getFloat3("position");
		Float3 axis = attributeTable.getFloat3("orientationAxis", Float3(0, 1, 0));
		float angle = attributeTable.getFloat("orientationAngle");
		Egg::Scene::FixedRigidBody::P fixedRigidBody =
			Egg::Scene::FixedRigidBody::Create();
		fixedRigidBody->Translate(position);
		fixedRigidBody->Rotate(axis, angle);
		Egg::Scene::Entity::P staticEntity =
			Egg::Scene::Entity::Create(multiMesh, fixedRigidBody);
		AddEntity(staticEntity);
		return staticEntity;
	}
	catch (std::exception exception) { ExitWithErrorMessage(exception); }
}

Egg::Cam::FirstPerson::P Script::ScriptedApp::CreateFirstPersonCam(luabind::object nil, luabind::object attributes)
{
	LuaTable attributeTable(attributes, "FirstPersonCam");
	try
	{
		using namespace Egg::Math;
		Egg::Cam::FirstPerson::P firstPersonCam = Egg::Cam::FirstPerson::Create()
			->SetView(attributeTable.getFloat3("position", Float3::Zero), attributeTable.getFloat3("position", -Float3::UnitZ))
			->SetProj(attributeTable.getFloat("fov", 1.2), attributeTable.getFloat("aspect", 1), attributeTable.getFloat("front", 0.1), attributeTable.getFloat("back", 1000.0))
			->SetSpeed(attributeTable.getFloat("speed", 5));

		cameras.push_back(firstPersonCam);
		return firstPersonCam;
	}
	catch (std::exception exception) { ExitWithErrorMessage(exception); }
}

Egg::Cam::Fixed::P Script::ScriptedApp::CreateFixedCam(luabind::object nil, luabind::object attributes)
{
	LuaTable attributeTable(attributes, "FixedCam");
	try
	{
		using namespace Egg::Math;
		Egg::Cam::Fixed::P fixedCam = Egg::Cam::Fixed::Create(
			attributeTable.get<Egg::Scene::Entity>("owner"),
			attributeTable.getFloat3("position", Float3::Zero),
			attributeTable.getFloat3("ahead", Float3::UnitX),
			attributeTable.getFloat3("up", Float3::UnitY),
			attributeTable.getFloat("fov", 1.2),
			attributeTable.getFloat("aspect", 1),
			attributeTable.getFloat("front", 0.1),
			attributeTable.getFloat("back", 1000.0)
		);

		cameras.push_back(fixedCam);
		return fixedCam;
	}
	catch (std::exception exception) { ExitWithErrorMessage(exception); }
}

void Script::ScriptedApp::RunScript(const std::string& luaFilename)
{
	std::string filepath = "Lua/" + luaFilename;
	int s = luaL_dofile(luaState, filepath.c_str());

	if (s != 0)
	{
		std::string errs = lua_tostring(luaState, -1);
		MessageBoxA(NULL, errs.c_str(), "Script error!", MB_OK);
		exit(-1);
	}
}