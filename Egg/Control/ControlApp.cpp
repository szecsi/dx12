#include "ControlApp.h"
#include "Egg/Script/LuaTable.h"
#include "Egg/Scene/Entity.h"
#include "Egg/Physics/PhysicsMathConversions.h"
#include "Egg/Physics/Model.h"
#include "Egg/Physics/Material.h"
#include "Egg/Physics/PhysicsRigidBody.h"
#include "Egg/Physics/PxEnumReflections.h"
#include "Egg/Script/luabindGetPointer.h"
#include "LuaControlState.h"

extern "C"
{
#include "lua.h"
#include "lualib.h"
}
#include "luabind/luabind.hpp"
#include "luabind/adopt_policy.hpp"


void Egg::Control::ControlApp::LoadAssets() {
	__super::LoadAssets();
	using namespace luabind;
	module(luaState)
	[
		class_<Egg::Control::ControlApp, Egg::Physics::PhysicsApp>("ControlApp")
		.def("ControlledEntity", &Egg::Control::ControlApp::CreateControlledEntity)
		.def("addForceAndTorque", &Egg::Control::ControlApp::AddForceAndTorque)
		.def("addForceAndTorqueForTarget", &Egg::Control::ControlApp::AddForceAndTorqueForTarget)
		.def("spawn", &Egg::Control::ControlApp::SpawnControlledEntity)
	];
	call_function<Egg::Control::ControlApp*>(luaState, "setEgg", this);

	scene->setSimulationEventCallback(&simulationEventCallback);

}

Egg::Scene::Entity::P Egg::Control::ControlApp::CreateControlledEntity(luabind::object nil, luabind::object attributes) {
	using namespace Egg::Script;
	using namespace Egg::Physics;
	LuaTable attributeTable(attributes, "ControlledEntity");
	try {
		using namespace Egg::Math;
		auto multiMesh = attributeTable.get<Egg::Mesh::Multi>("multiMesh");
		Float3 position = attributeTable.getFloat3("position");
		Float3 axis = attributeTable.getFloat3("orientationAxis", Float3(0, 1, 0));
		float angle = attributeTable.getFloat("orientationAngle");
		auto model = attributeTable.get<Egg::Physics::Model>("model");
		auto controlStateObj = attributeTable.getLuaBindObject("controlState");
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
		auto controlState = Egg::Control::LuaControlState::Create(physicsEntity, controlStateObj);
		physicsEntity->SetControlState(controlState);
		AddEntity(physicsEntity);
		return physicsEntity;
	}
	catch (std::exception exception) { ExitWithErrorMessage(exception); }
}

Egg::Math::Float4 quatMul(
	const Egg::Math::Float4& p,
	const Egg::Math::Float4& o) {
	return Egg::Math::Float4(
		p.y * o.z - p.z * o.y + p.w * o.x + p.x * o.w,
		p.z * o.x - p.x * o.z + p.w * o.y + p.y * o.w,
		p.x * o.y - p.y * o.x + p.w * o.z + p.z * o.w,
		p.w * o.w - (p.x * o.x + p.y * o.y + p.z * o.z));
}

void Egg::Control::ControlApp::SpawnControlledEntity(Egg::Scene::Entity::P parentEntity, luabind::object attributes) {
	using namespace Egg::Script;
	using namespace Egg::Physics;
	LuaTable attributeTable(attributes, "ControlledEntity");
	try {
		using namespace Egg::Math;
		auto multiMesh = attributeTable.get<Egg::Mesh::Multi>("multiMesh");
		Float3 position = attributeTable.getFloat3("position");
		Float3 axis = attributeTable.getFloat3("orientationAxis", Float3(0, 1, 0));
		float angle = attributeTable.getFloat("orientationAngle");
		auto model = attributeTable.get<Egg::Physics::Model>("model");
		auto controlStateObj = attributeTable.getLuaBindObject("controlState");
		// quaternion from axis and angle
		Float4 orientation = Float4(axis.Normalize() * sinf(angle / 2), cosf(angle / 2));

		auto parentModelMatrix = parentEntity->GetRigidBody()->GetModelMatrix();
		auto parentOrientation = parentEntity->GetRigidBody()->GetOrientation();

		position = (Float4(position, 1) * parentModelMatrix).xyz;
		orientation = quatMul(parentOrientation, orientation);

		Physics::PhysicsRigidBody::P rigidBody =
			Physics::PhysicsRigidBody::Create(
				scene, model, position, orientation
			);
		auto actor = rigidBody->GetActor();

		auto linearVelocity = attributeTable.getFloat3("linearVelocity", ~actor->getLinearVelocity());
		linearVelocity = (Float4(linearVelocity, 0) * parentModelMatrix).xyz;
		actor->setLinearVelocity(~linearVelocity);

		actor->setLinearDamping(attributeTable.getFloat("linearDamping", actor->getLinearDamping()));
		actor->setAngularDamping(attributeTable.getFloat("angularDamping", actor->getAngularDamping()));
		actor->setMaxAngularVelocity(attributeTable.getFloat("maxAngularVelocity", actor->getMaxAngularVelocity()));
	//	actor->setLinearVelocity(~attributeTable.getFloat3("linearVelocity", ~actor->getLinearVelocity()));
		actor->setAngularVelocity(~attributeTable.getFloat3("angularVelocity", ~actor->getAngularVelocity()));


		Scene::Entity::P physicsEntity =
			Scene::Entity::Create(multiMesh, rigidBody);
		actor->userData = physicsEntity.get();
		auto controlState = Egg::Control::LuaControlState::Create(physicsEntity, controlStateObj);
		physicsEntity->SetControlState(controlState);
		spawnedEntities.push_back(physicsEntity);

	}
	catch (std::exception exception) { ExitWithErrorMessage(exception); }
}

void Egg::Control::ControlApp::Update(float dt, float T) {
	using namespace luabind;
	try
	{
		globals(luaState)["dt"] = dt;
		object keysPressed = globals(luaState)["keysPressed"];
		if (type(keysPressed) == LUA_TTABLE)
		{
			int iKey = 0;
			for (iterator i(keysPressed), end; i != end; ++i, ++iKey)
			{
				std::string kks = luabind::object_cast<std::string>(i.key());
				int keycode = kks.at(0);
				if (kks == "VK_SPACE")   keycode = VK_SPACE;
				if (kks == "VK_NUMPAD0") keycode = VK_NUMPAD0;
				if (kks == "VK_NUMPAD1") keycode = VK_NUMPAD1;
				if (kks == "VK_NUMPAD2") keycode = VK_NUMPAD2;
				if (kks == "VK_NUMPAD3") keycode = VK_NUMPAD3;
				if (kks == "VK_NUMPAD4") keycode = VK_NUMPAD4;
				if (kks == "VK_NUMPAD5") keycode = VK_NUMPAD5;
				if (kks == "VK_NUMPAD6") keycode = VK_NUMPAD6;
				if (kks == "VK_NUMPAD7") keycode = VK_NUMPAD7;
				if (kks == "VK_NUMPAD8") keycode = VK_NUMPAD8;
				if (kks == "VK_NUMPAD9") keycode = VK_NUMPAD9;

				*i = this->keysPressed[keycode] ? true : false;
			}
		}
	}
	catch (luabind::error exception) {
		std::string errs = lua_tostring(luaState, -1);
		MessageBoxA(NULL, errs.c_str(), "Script error!", MB_OK);
		exit(-1);
	}
	catch (std::exception exception) { ExitWithErrorMessage(exception); }

	__super::Update(dt, T);
	entities.insert(entities.end(), spawnedEntities.begin(), spawnedEntities.end());
	spawnedEntities.clear();

}

void Egg::Control::ControlApp::AddForceAndTorque(Egg::Scene::Entity::P entity, luabind::object attributes){
	using namespace Egg::Script;
	using namespace Egg::Physics;
	LuaTable attributeTable(attributes, "AddForceAndTorque");
	try {
		auto rigid = entity->GetRigidBody();
		rigid->AddForce((attributeTable.getFloat4("force")  * rigid->GetRotationMatrix() ).xyz  );
		rigid->AddTorque((attributeTable.getFloat4("torque")  * rigid->GetRotationMatrix() ).xyz );
	}
	catch (std::exception exception) { ExitWithErrorMessage(exception); }
}

bool Egg::Control::ControlApp::AddForceAndTorqueForTarget(Egg::Scene::Entity::P entity, luabind::object attributes) {
	using namespace Egg::Script;
	using namespace Egg::Physics;
	using namespace Egg::Math;
	LuaTable attributeTable(attributes, "AddForceAndTorque");
	try {
		float proximityRadius = attributeTable.getFloat("proximityRadius");
		float maxForce = attributeTable.getFloat("maxForce");
		float maxTorque = attributeTable.getFloat("maxTorque");
		Float3 markPosition = attributeTable.getFloat3("position");
		auto mark = attributeTable.get<Egg::Scene::Entity>("mark", nullptr);
		if (mark)
		{
			markPosition = (Float4(markPosition, 1) * mark->GetRigidBody()->GetModelMatrix()).xyz;
		}
		auto rigid = entity->GetRigidBody();

		Float3 markDiff = markPosition - rigid->GetPosition();
		float markDist = markDiff.Length();
		Float3 markDir = markDiff / markDist;
		Float3 ahead = (Float4(1, 0, 0, 0) * rigid->GetRotationMatrix()).xyz;
		rigid->AddForce(ahead * std::max(markDir.Dot(ahead), 0.0f) * maxForce);
		rigid->AddTorque(ahead.Cross(markDir) * maxTorque);

		if (markDist < proximityRadius)	return true;
		return false;
	}
	catch (std::exception exception) { ExitWithErrorMessage(exception); }
}

void  Egg::Control::ControlApp::SimulationEventCallback::onContact(
	const physx::PxContactPairHeader& pairHeader,
	const physx::PxContactPair* pairs,
	physx::PxU32 nbPairs)
{
	using namespace physx;
	if (pairHeader.flags & (PxContactPairHeaderFlag::eREMOVED_ACTOR_0 | PxContactPairHeaderFlag::eREMOVED_ACTOR_1))
		return;

	for (PxU32 i = 0; i < nbPairs; i++)
	{
		const PxContactPair& cp = pairs[i];

		if (cp.events & PxPairFlag::eNOTIFY_TOUCH_FOUND)
		{
			if (pairHeader.actors[0]->userData == NULL)
				return;
			if (pairHeader.actors[1]->userData == NULL)
				return;
			auto entity0 = ((Egg::Scene::Entity*)pairHeader.actors[0]->userData)->GetShared();
			auto entity1 = ((Egg::Scene::Entity*)pairHeader.actors[1]->userData)->GetShared();

			if (entity0 && entity1) {
				if(entity0->GetControlState())
				entity0->GetControlState()->onContact(entity1);
				if(entity1->GetControlState())
				entity1->GetControlState()->onContact(entity0);
			}
		}
	}
}