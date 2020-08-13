#pragma once
#include "../Common.h"
#include "Egg/Scene/RigidBody.h"
#include "physx/PxPhysicsAPI.h"
#include "Model.h"

namespace Egg { namespace Physics
{
	GG_SUBCLASS(PhysicsRigidBody, Egg::Scene::RigidBody)
	protected:
		physx::PxRigidDynamic* actor;

		/// Private constructor. Initializes the model transformation to indentity.
		/// @param shadedMesh the ShadedMesh which the Entity uses for displaying itself.
		PhysicsRigidBody(physx::PxScene* scene,
			Egg::Physics::Model::P model,
			Egg::Math::Float3 position,
			Egg::Math::Float4 orientation);
	public:

		virtual ~PhysicsRigidBody();

		/// Returns the model matrix. To be used for rendering, and positioning of light sources and cameras attached to the entity.
		virtual Egg::Math::Float4x4 GetModelMatrix() override;

		/// Returns the inverse of the model matrix.
		virtual Egg::Math::Float4x4 GetModelMatrixInverse() override;

		/// Returns reference point in world space.
		virtual Egg::Math::Float3 GetPosition() override;

		/// Returns orientation as a quaternion in world space.
		virtual Egg::Math::Float4 GetOrientation() override;

		/// Returns the rotation matrix.
		virtual Egg::Math::Float4x4 GetRotationMatrix() override;

		/// Returns the inverse of the rotation matrix. To be used for the view transformation of attached cameras.
		virtual Egg::Math::Float4x4 GetRotationMatrixInverse() override;

		physx::PxRigidDynamic* GetActor(){return actor;}

		void AddForce(Egg::Math::Float3 force) override;
		void AddTorque(Egg::Math::Float3 torque) override;

	GG_ENDCLASS

}}
