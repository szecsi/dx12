#pragma once
#include "Script/ScriptedApp.h"
#include "PxPhysicsAPI.h"
#include "pvd/PxVisualDebugger.h"
#include "Physics/PhysicsEntity.h"
#include "Physics/RagdollEntity.h"

namespace Egg { namespace Physics
{
	/// Application class with scene management
	class PhysicsApp : public Egg::Script::ScriptedApp
	{
	protected:
		physx::PxFoundation* foundation;
		physx::PxPhysics* physics;
		physx::pxtask::CpuDispatcher* cpuDispatcher;
		PVD::PvdConnection* pvdConnection;
		physx::PxScene* scene;

		double densityForEntityBeingAdded;
		Egg::Scene::Directory<PhysicsEntity> bonesOfRagdollBeingAdded;

		double timeRemainingOfTimestep;
		double timestep;

		/// Local typedef for an associative container for NxMaterial objects.
		typedef std::map<std::string, physx::PxMaterial*> PhysicsMaterialDirectory;
		/// NxMaterial references accessible by name.
		PhysicsMaterialDirectory	physicsMaterials;

	public:
		PhysicsApp(ID3D11Device* device);
		virtual HRESULT createResources();
		virtual HRESULT releaseResources();

		virtual void animate(double dt, double t);
		virtual bool processMessage( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
		virtual void render(ID3D11DeviceContext* context);

		void addPhysicsMaterial(luabind::object nil, luabind::object attributes);
		void addPhysicsEntity(luabind::object nil, luabind::object attributes, luabind::object initializer);
		void addShapeToPhysicsEntity(PhysicsEntity::P physicsEntity, luabind::object attributes);
		void setDynamicsForPhysicsEntity(PhysicsEntity::P physicsEntity, luabind::object attributes);

		void addRagdoll(luabind::object nil, luabind::object attributes, luabind::object initializer);
		void addBoneToRagdoll(RagdollEntity::P ragdoll, luabind::object attributes, luabind::object initializer);
	};

}}