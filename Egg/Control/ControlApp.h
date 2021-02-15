#pragma once
#include "../Common.h"
#include "Egg/Physics/PhysicsApp.h"

namespace Egg {
	namespace Control {
		/// Application class with scene management
		GG_SUBCLASS(ControlApp, Egg::Physics::PhysicsApp)

			bool keysPressed[0xff];
			std::vector<Egg::Scene::Entity::P> spawnedEntities;

			class SimulationEventCallback : public physx::PxSimulationEventCallback
			{
				Egg::Physics::PhysicsApp* app;
			public:
				SimulationEventCallback(Egg::Physics::PhysicsApp*) :app(app) {}
				void  onConstraintBreak(physx::PxConstraintInfo* constraints, physx::PxU32 count) {}
				void  onWake(physx::PxActor** actors, physx::PxU32 count) {}
				void  onSleep(physx::PxActor** actors, physx::PxU32 count) {}
				void  onContact(const physx::PxContactPairHeader& pairHeader, const physx::PxContactPair* pairs, physx::PxU32 nbPairs);
				void  onTrigger(physx::PxTriggerPair* pairs, physx::PxU32 count) {}
				void  onAdvance(const physx::PxRigidBody* const* bodyBuffer, const physx::PxTransform* poseBuffer, const physx::PxU32 count) {}
			} simulationEventCallback;
protected:
	ControlApp() :simulationEventCallback(this) {}

public:
		virtual void LoadAssets() override;
		virtual void Update(float dt, float T) override;

		void ProcessMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override
		{
			__super::ProcessMessage(hWnd, uMsg, wParam, lParam);

			if (uMsg == WM_KEYDOWN)
				keysPressed[wParam] = true;
			else if (uMsg == WM_KEYUP)
				keysPressed[wParam] = false;
			else if (uMsg == WM_KILLFOCUS)
			{
				for (unsigned int i = 0; i < 0xff; i++)
					keysPressed[i] = false;
			}

		}

		Egg::Scene::Entity::P CreateControlledEntity(luabind::object nil, luabind::object attributes);
		void AddForceAndTorque(Egg::Scene::Entity::P entity, luabind::object attributes);
		bool AddForceAndTorqueForTarget(Egg::Scene::Entity::P entity, luabind::object attributes);

		void SpawnControlledEntity(Egg::Scene::Entity::P parentEntity, luabind::object attributes);

		GG_ENDCLASS
	}
}
