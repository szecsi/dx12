#include "DXUT.h"
#include "Physics/PhysicsApp.h"
#include "Script/LuaTable.h"
#include "Physics/PhysicsMathConversions.h"
#include "Physics/PhysicsEntity.h"
#include "Physics/RagdollEntity.h"
#include "Physics/PxEnumReflections.h"

using namespace Egg;
using namespace physx;

Physics::PhysicsApp::PhysicsApp(ID3D11Device* device)
	:ScriptedApp(device)
{
	using namespace luabind;

	module(luaState)
	[
		class_<Physics::PhysicsEntity>("PhysicsEntity"),
		class_<Physics::RagdollEntity>("RagdollEntity"),

		class_<Physics::PhysicsApp, Script::ScriptedApp>("PhysicsApp")
			.def("PhysicsMaterial", &Physics::PhysicsApp::addPhysicsMaterial)
			.def("PhysicsEntity", &Physics::PhysicsApp::addPhysicsEntity)
			.def("Shape", &Physics::PhysicsApp::addShapeToPhysicsEntity)
			.def("Dynamics", &Physics::PhysicsApp::setDynamicsForPhysicsEntity)
			.def("Ragdoll", &Physics::PhysicsApp::addRagdoll)
			.def("Bone", &Physics::PhysicsApp::addBoneToRagdoll)
	];

	call_function<Physics::PhysicsApp*>(luaState, "setEgg", this);

	PxEnumReflections::initialize();

	timestep = 0.02;
	timeRemainingOfTimestep = 0.0;
}

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


HRESULT Physics::PhysicsApp::createResources()
{
	__super::createResources();

	static PxDefaultErrorCallback gDefaultErrorCallback;
	static PxDefaultAllocator gDefaultAllocatorCallback;
	static PxSimulationFilterShader gDefaultFilterShader = PxDefaultSimulationFilterShader;
	foundation = PxCreateFoundation(PX_PHYSICS_VERSION, gDefaultAllocatorCallback, gDefaultErrorCallback);

	physics = PxCreatePhysics(PX_PHYSICS_VERSION, *foundation, PxTolerancesScale(), false);

	// check if PvdConnection manager is available on this platform
	if(physics->getPvdConnectionManager() != NULL)
	{
		// setup connection parameters
		const char*     pvd_host_ip = "127.0.0.1";  // IP of the PC which is running PVD
		int             port        = 5425;         // TCP port to connect to, where PVD is listening
		unsigned int    timeout     = 100;          // timeout in milliseconds to wait for PVD to respond,
													// consoles and remote PCs need a higher timeout.
		PxVisualDebuggerConnectionFlags connectionFlags = PxVisualDebuggerExt::getAllConnectionFlags();

		// and now try to connect
		pvdConnection = PxVisualDebuggerExt::createConnection(physics->getPvdConnectionManager(),
			pvd_host_ip, port, timeout, connectionFlags);

	}
	else
		pvdConnection = NULL;

	PxSceneDesc sceneDesc(physics->getTolerancesScale());
	sceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f);
	if(!sceneDesc.cpuDispatcher)
	{
		cpuDispatcher = PxDefaultCpuDispatcherCreate(1);
		sceneDesc.cpuDispatcher    = cpuDispatcher;
	}
	if(!sceneDesc.filterShader)
		sceneDesc.filterShader    = allContactsFilterShader;//gDefaultFilterShader;

	scene = physics->createScene(sceneDesc);
	
	return S_OK;
}

HRESULT Physics::PhysicsApp::releaseResources()
{
	HRESULT hr = __super::releaseResources();

	if (pvdConnection)
		pvdConnection->release();

	return hr;
}

void Physics::PhysicsApp::render(ID3D11DeviceContext* context)
{	
	__super::render(context);
}

void Physics::PhysicsApp::animate(double dt, double t)
{

	timeRemainingOfTimestep -= dt;
	while(timeRemainingOfTimestep < 0)
	{
		timeRemainingOfTimestep += timestep;
		__super::animate(timestep, t);
		scene->simulate(timestep);
		scene->fetchResults(true);
	}
}

bool Physics::PhysicsApp::processMessage( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return __super::processMessage(hWnd, uMsg, wParam, lParam);
}

void Physics::PhysicsApp::addPhysicsMaterial(luabind::object nil, luabind::object attributes)
{
	Script::LuaTable attributeTable(attributes, "PhysicsMaterial");
	try
	{
		using namespace Egg::Math;
		std::string name = attributeTable.getString("name");
		PxMaterial* material = physics->createMaterial(
			attributeTable.getFloat("staticFriction", 0.5),
			attributeTable.getFloat("dynamicFriction", 0.5),
			attributeTable.getFloat("restitution", 0.5)
			);

		physicsMaterials[name] = material;
	}
	catch(Egg::HrException exception){ exitWithErrorMessage(exception); }
}

void Physics::PhysicsApp::addPhysicsEntity(luabind::object nil, luabind::object attributes, luabind::object initializer)
{
	using namespace Egg::Script;
	LuaTable attributeTable(attributes, "PhysicsEntity");
	try
	{
		std::string entityName = attributeTable.getString("name");
		std::string multiMeshName = attributeTable.getString("multiMesh");
		using namespace Egg::Math;
		float3 position = attributeTable.getFloat3("position");
		float3 axis = attributeTable.getFloat3("orientationAxis", float3(0, 1, 0));
		float angle = attributeTable.getFloat("orientationAngle");
		Scene::Directory<Mesh::Multi>::iterator iMultiMesh = multiMeshes.find(multiMeshName);
		if(iMultiMesh == multiMeshes.end())
			attributeTable.throwError(std::string("Unknown MultiMesh '") +multiMeshName+ "'.");
		Physics::PhysicsEntity::P physicsEntity = Physics::PhysicsEntity::create(scene, position, float4::quatAxisAngle(axis, angle), iMultiMesh->second);
		densityForEntityBeingAdded = 1.0;
		initializer(physicsEntity);
		if(PxRigidDynamic* rigidDynamic = physicsEntity->getActor()->isRigidDynamic())
			PxRigidBodyExt::updateMassAndInertia(*rigidDynamic, densityForEntityBeingAdded);
		physicsEntity->getActor()->userData = physicsEntity.get();
		scene->addActor(*physicsEntity->getActor());
		entities[entityName] = physicsEntity;
	}
	catch(Egg::HrException exception){ exitWithErrorMessage(exception); }
}

void Physics::PhysicsApp::addShapeToPhysicsEntity(PhysicsEntity::P physicsEntity, luabind::object attributes)
{
	using namespace Egg::Script;
	LuaTable attributeTable(attributes, "Shape");
	try
	{
		using namespace Egg::Math;
		using namespace physx;
		float3 position = attributeTable.getFloat3("position");
		float3 axis = attributeTable.getFloat3("orientationAxis", float3(0, 1, 0));
		axis = axis.normalize();
		float angle = attributeTable.getFloat("orientationAngle");

		std::string materialName = attributeTable.getString("material");
		PhysicsMaterialDirectory::iterator iMaterial = physicsMaterials.find(materialName);
		if(iMaterial == physicsMaterials.end())
			attributeTable.throwError(std::string("Unknown physics material '") +materialName+ "'.");
		PxGeometry* geometry = NULL;
		PxGeometryType::Enum e = attributeTable.getEnum<PxGeometryType::Enum>("geometryType", PxGeometryType::eINVALID);
		switch(e)
		{
			case PxGeometryType::eBOX:
				geometry = new PxBoxGeometry( ~attributeTable.getFloat3("halfExtents", float3(1, 1, 1)) );
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
		physicsEntity->getActor()->createShape(
			*geometry, *iMaterial->second, PxTransform(~position, PxQuat(angle, ~axis)));
		delete geometry;
	}
	catch(Egg::HrException exception){ exitWithErrorMessage(exception); }
}

void Physics::PhysicsApp::setDynamicsForPhysicsEntity(PhysicsEntity::P physicsEntity, luabind::object attributes)
{
	using namespace Egg::Script;
	LuaTable attributeTable(attributes, "Dynamics");
	try
	{
		PxRigidDynamic* rigidDynamic = physicsEntity->makeActorDynamic(scene);
		rigidDynamic->setActorFlags(attributeTable.getEnumCombination<PxActorFlag::Enum, PxActorFlags>("actorFlags"));

		rigidDynamic->setLinearDamping( attributeTable.getFloat("linearDamping", rigidDynamic->getLinearDamping() ));
		rigidDynamic->setAngularDamping( attributeTable.getFloat("angularDamping", rigidDynamic->getAngularDamping() ));
		rigidDynamic->setMaxAngularVelocity( attributeTable.getFloat("maxAngularVelocity", rigidDynamic->getMaxAngularVelocity() ));
		rigidDynamic->setRigidDynamicFlags( attributeTable.getEnumCombination<PxRigidDynamicFlag::Enum, PxRigidDynamicFlags>("rigidDynamicFlags", rigidDynamic->getRigidDynamicFlags() ));
		rigidDynamic->setLinearVelocity( ~attributeTable.getFloat3("linearVelocity", ~rigidDynamic->getLinearVelocity() ));
		rigidDynamic->setAngularVelocity( ~attributeTable.getFloat3("angularVelocity", ~rigidDynamic->getAngularVelocity() ));
		float density = attributeTable.getFloat("density", -1.0);
		if(density > 0)
			densityForEntityBeingAdded = density;
	}
	catch(Egg::HrException exception){ exitWithErrorMessage(exception); }
}


void Physics::PhysicsApp::addRagdoll(luabind::object nil, luabind::object attributes, luabind::object initializer)
{
	using namespace Egg::Script;
	LuaTable attributeTable(attributes, "Ragdoll");
	try
	{
		std::string entityName = attributeTable.getString("name");
		std::string multiMeshName = attributeTable.getString("multiMesh");
		using namespace Egg::Math;
		float3 position = attributeTable.getFloat3("position");
		float3 axis = attributeTable.getFloat3("orientationAxis", float3(0, 1, 0));
		float angle = attributeTable.getFloat("orientationAngle");
		Scene::Directory<Mesh::Multi>::iterator iMultiMesh = multiMeshes.find(multiMeshName);
		if(iMultiMesh == multiMeshes.end())
			attributeTable.throwError(std::string("Unknown MultiMesh '") +multiMeshName+ "'.");
		Physics::RagdollEntity::P ragdollEntity = Physics::RagdollEntity::create(position, float4::quatAxisAngle(axis, angle), iMultiMesh->second);
		densityForEntityBeingAdded = 1.0;
		initializer(ragdollEntity);
		entities[entityName] = ragdollEntity;
		//TODO move all actors by position and orientation
		bonesOfRagdollBeingAdded.clear();
	}
	catch(Egg::HrException exception){ exitWithErrorMessage(exception); }
}

void Physics::PhysicsApp::addBoneToRagdoll(RagdollEntity::P ragdoll, luabind::object attributes, luabind::object initializer)
{
	using namespace Egg::Script;
	using namespace Egg::Math;
	LuaTable attributeTable(attributes, "Bone");
	try
	{
		std::string entityName = attributeTable.getString("name");
		using namespace Egg::Math;
		float3 position = attributeTable.getFloat3("position");
		float3 orientation = attributeTable.getFloat3("orientation", float3(0, 0, 0));
		float4 quat = float4(orientation, -sqrtf(std::max<float>(0, 1 - orientation.dot(orientation))) );

		Physics::PhysicsEntity::P physicsEntity = Physics::PhysicsEntity::create(scene, position, quat, Egg::Mesh::Multi::P());
		densityForEntityBeingAdded = 1.0;
		initializer(physicsEntity);
		if(PxRigidDynamic* rigidDynamic = physicsEntity->getActor()->isRigidDynamic())
			PxRigidBodyExt::updateMassAndInertia(*rigidDynamic, densityForEntityBeingAdded);
		physicsEntity->getActor()->userData = physicsEntity.get();
		scene->addActor(*physicsEntity->getActor());
		ragdoll->addBoneEntity( physicsEntity );

		if(bonesOfRagdollBeingAdded.size() > 0) //root bone needs no parent
		{
			std::string parentBoneName = attributeTable.getString("parent");
			Egg::Scene::Directory<PhysicsEntity>::iterator iParentBone = bonesOfRagdollBeingAdded.find(parentBoneName);
			if(iParentBone == bonesOfRagdollBeingAdded.end())
				attributeTable.throwError(std::string("Unknown parent bone '") +parentBoneName+ "'.");
			float4x4 m = physicsEntity->getModelMatrix();
			float4x4 p = iParentBone->second->getModelMatrixInverse();

			float3 mpos = physicsEntity->getPosition();
			float3 ppos = iParentBone->second->getPosition();

			float4x4 fromChildBoneToParentBoneTransform = m*p;
			PxTransform pixi( PxMat44( (float(&)[16])fromChildBoneToParentBoneTransform) );
			PxD6Joint* joint = PxD6JointCreate(*physics, 
				iParentBone->second->getActor(), 
					pixi,
//					PxTransform( ~(mpos - ppos)),
				physicsEntity->getActor(), PxTransform::createIdentity());
			joint->setMotion(PxD6Axis::eX, PxD6Motion::eLOCKED);
			joint->setMotion(PxD6Axis::eY, PxD6Motion::eLOCKED);
			joint->setMotion(PxD6Axis::eZ, PxD6Motion::eLOCKED);
			joint->setMotion(PxD6Axis::eTWIST, PxD6Motion::eLOCKED);
			joint->setMotion(PxD6Axis::eSWING1, PxD6Motion::eLIMITED);
			joint->setMotion(PxD6Axis::eSWING2, PxD6Motion::eLIMITED);
			joint->setSwingLimit( PxJointLimitCone(1, 1, 0.0) );
		}

		bonesOfRagdollBeingAdded[entityName] = physicsEntity;
	}
	catch(Egg::HrException exception){ exitWithErrorMessage(exception); }
}
