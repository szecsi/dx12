#include "PhysicsApp.h"
#include "Egg/Script/LuaTable.h"
#include "PhysicsMathConversions.h"
#include "Model.h"
#include "Material.h"
#include "PxEnumReflections.h"

using namespace Egg;
using namespace physx;

PxFilterFlags allContactsFilterShader(
	PxFilterObjectAttributes attributes0, PxFilterData filterData0,
	PxFilterObjectAttributes attributes1, PxFilterData filterData1,
	PxPairFlags& pairFlags, const void* constantBlock, PxU32 constantBlockSize)
{
	// generate contacts for all that were not filtered above
	pairFlags = PxPairFlag::eCONTACT_DEFAULT;
	pairFlags |= PxPairFlag::eNOTIFY_TOUCH_FOUND;

	return PxFilterFlag::eDEFAULT;
}

void Physics::PhysicsApp::LoadAssets() {
	__super::LoadAssets();

	using namespace luabind;

	module(luaState)
	[
		class_<Physics::Model>("CPhysicsModel"),
		class_<Physics::Material>("CPhysicsMaterial"),

		class_<Physics::PhysicsApp, Script::ScriptedApp>("PhysicsApp")
			.def("PhysicsMaterial", &Physics::PhysicsApp::CreatePhysicsMaterial)
			.def("PhysicsEntity", &Physics::PhysicsApp::CreatePhysicsEntity)
			.def("PhysicsModel", &Physics::PhysicsApp::CreatePhysicsModel)
			.def("Shape", &Physics::PhysicsApp::AddShapeToModel)
	];

	call_function<Physics::PhysicsApp*>(luaState, "setEgg", this);

	PxEnumReflections::initialize();

	timestep = 0.02;
	timeRemainingOfTimestep = 0.0;

	static PxDefaultErrorCallback gDefaultErrorCallback;
	static PxDefaultAllocator gDefaultAllocatorCallback;
	foundation = PxCreateFoundation(PX_PHYSICS_VERSION, gDefaultAllocatorCallback, gDefaultErrorCallback);

	physics = PxCreatePhysics(PX_PHYSICS_VERSION, *foundation, PxTolerancesScale(), false);

	PxSceneDesc sceneDesc(physics->getTolerancesScale());
	sceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f);
	if (!sceneDesc.cpuDispatcher) {
		sceneDesc.cpuDispatcher = PxDefaultCpuDispatcherCreate(1);
	}
	if (!sceneDesc.filterShader)
		sceneDesc.filterShader = allContactsFilterShader;

	scene = physics->createScene(sceneDesc);
}

void Physics::PhysicsApp::ReleaseResources()
{
	__super::ReleaseResources();

	physics->release();
	foundation->release();
}

void Physics::PhysicsApp::Update(float dt, float T) {
	timeRemainingOfTimestep -= dt;
	while(timeRemainingOfTimestep < 0)
	{
		timeRemainingOfTimestep += timestep;
		//__super::animate(timestep, t);
		scene->simulate(timestep);
		scene->fetchResults(true);
	}

	__super::Update(dt, T);
}

Physics::Material::P Physics::PhysicsApp::CreatePhysicsMaterial(luabind::object nil, luabind::object attributes) {
	Script::LuaTable attributeTable(attributes, "PhysicsMaterial");
	try {
		using namespace Egg::Math;
		PxMaterial* material = physics->createMaterial(
			attributeTable.getFloat("staticFriction", 0.5),
			attributeTable.getFloat("dynamicFriction", 0.5),
			attributeTable.getFloat("restitution", 0.5)
			);

		return Physics::Material::Create(material);
	}
	catch (std::exception exception) { ExitWithErrorMessage(exception); }
}

Egg::Scene::Entity::P Physics::PhysicsApp::CreatePhysicsEntity(luabind::object nil, luabind::object attributes) {
	using namespace Egg::Script;
	LuaTable attributeTable(attributes, "PhysicsEntity");
	try {
		using namespace Egg::Math;
		auto multiMesh = attributeTable.get<Egg::Mesh::Multi>("multiMesh");
		Float3 position = attributeTable.getFloat3("position");
		Float3 axis = attributeTable.getFloat3("orientationAxis", Float3(0, 1, 0));
		float angle = attributeTable.getFloat("orientationAngle");
		auto model = attributeTable.get<Egg::Physics::Model>("model");
		// quaternion from axis and angle
		Float4 orientation = Float4(axis.Normalize() * sinf(angle / 2), cosf(angle / 2));
		Physics::PhysicsRigidBody::P rigidBody =
			Physics::PhysicsRigidBody::Create(
				scene, model, position, orientation
			);
		auto actor = rigidBody->GetActor();

		actor->setLinearDamping(attributeTable.getFloat("linearDamping", actor->getLinearDamping()));
		actor->setAngularDamping(attributeTable.getFloat("angularDamping", actor->getAngularDamping()));
		actor->setMaxAngularVelocity(attributeTable.getFloat("maxAngularVelocity", actor->getMaxAngularVelocity()));
		actor->setLinearVelocity(~attributeTable.getFloat3("linearVelocity", ~actor->getLinearVelocity()));
		actor->setAngularVelocity(~attributeTable.getFloat3("angularVelocity", ~actor->getAngularVelocity()));

		Scene::Entity::P physicsEntity = 
			Scene::Entity::Create(multiMesh, rigidBody);
		actor->userData = physicsEntity.get();
		AddEntity(physicsEntity);
		return physicsEntity;
	}
	catch (std::exception exception) { ExitWithErrorMessage(exception); }
}

Egg::Physics::Model::P Physics::PhysicsApp::CreatePhysicsModel(luabind::object nil, luabind::object attributes, luabind::object initializer) {
	Script::LuaTable attributeTable(attributes, "PhysicsModel");
	try {
		using namespace Egg::Math;
		auto model = Physics::Model::Create();
		model->density = attributeTable.getFloat("density", 1.0);
		model->actorFlags = attributeTable.getEnumCombination<PxActorFlag::Enum, PxActorFlags>("actorFlags");
		model->rigidBodyFlags = attributeTable.getEnumCombination<PxRigidBodyFlag::Enum, PxRigidBodyFlags>("rigidBodyFlags");
		initializer(model);

		return model;
	}
	catch (std::exception exception) { ExitWithErrorMessage(exception); }
}


void Physics::PhysicsApp::AddShapeToModel(Model::P model, luabind::object attributes) {
	using namespace Egg::Script;
	LuaTable attributeTable(attributes, "Shape");
	try {
		using namespace Egg::Math;
		using namespace physx;
		Float3 position = attributeTable.getFloat3("position");
		Float3 axis = attributeTable.getFloat3("orientationAxis", Float3(0, 1, 0));
		axis = axis.Normalize();
		float angle = attributeTable.getFloat("orientationAngle");

		auto material = attributeTable.get<Egg::Physics::Material>("material");
		PxGeometry* geometry = NULL;
		PxGeometryType::Enum e = attributeTable.getEnum<PxGeometryType::Enum>("geometryType", PxGeometryType::eINVALID);
		switch(e) {
			case PxGeometryType::eBOX:
				geometry = new PxBoxGeometry( ~attributeTable.getFloat3("halfExtents", Float3(1, 1, 1)) );
				break;
			case PxGeometryType::eCAPSULE:
				geometry = new PxCapsuleGeometry( attributeTable.getFloat("radius"), attributeTable.getFloat("halfHeight") );
				break;
			case PxGeometryType::ePLANE:
				geometry = new PxPlaneGeometry( );
				break;
			case PxGeometryType::eSPHERE:
				geometry = new PxSphereGeometry( attributeTable.getFloat("radius") );
				break;
			case PxGeometryType::eINVALID:
				attributeTable.throwError("Geometry type not specified!");
			default:
				attributeTable.throwError("Geometry type not supported!");
		}
		auto shape = physics->createShape(
			*geometry, *material->GetPxMaterial());
		shape->setLocalPose(
			PxTransform(~position, PxQuat(angle, ~axis)));
		delete geometry;
		model->addShape(shape);
	}
	catch (std::exception exception) { ExitWithErrorMessage(exception); }
}

