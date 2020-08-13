#pragma once
#include "../Common.h"
#include "Egg/Script/ScriptedApp.h"
#include "physx/PxPhysicsAPI.h"
//#include "physx/pvd/PxVisualDebugger.h"
#include "Model.h"
#include "Material.h"
#include "PhysicsRigidBody.h"
//#include "RagdollEntity.h"

namespace Egg {
	namespace Physics {
		/// Application class with scene management
		GG_SUBCLASS(PhysicsApp, Egg::Script::ScriptedApp)
		protected:
			physx::PxFoundation* foundation;
			physx::PxPhysics* physics;
//			PVD::PvdConnection* pvdConnection;
			physx::PxScene* scene;

			double timeRemainingOfTimestep;
			double timestep;

		public:
			void ReleaseResources() override;
			void LoadAssets() override;

			virtual void Update(float dt, float T) override;

			Egg::Physics::Material::P CreatePhysicsMaterial(luabind::object nil, luabind::object attributes);
			Egg::Scene::Entity::P CreatePhysicsEntity(luabind::object nil, luabind::object attributes);
			Egg::Physics::Model::P CreatePhysicsModel(luabind::object nil, luabind::object attributes, luabind::object initializer);
			void AddShapeToModel(Model::P model, luabind::object attributes);

	GG_ENDCLASS
	}
}