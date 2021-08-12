#pragma once
#include "Scene/Entity.h"
#include "Mesh/Multi.h"
#include "PxPhysicsAPI.h"
#include <boost/enable_shared_from_this.hpp>


namespace Egg { namespace Physics
{
	class PhysicsEntity : public Egg::Scene::Entity, public boost::enable_shared_from_this<PhysicsEntity>
	{
	protected:
		physx::PxRigidActor* actor;

		/// Private constructor. Initializes the model transformation to indentity.
		/// @param shadedMesh the ShadedMesh which the Entity uses for displaying itself.
		PhysicsEntity(physx::PxScene* scene, Egg::Math::float3 position, Egg::Math::float4 orientation, Mesh::Multi::P multiMesh);
	public:
		/// Local shorthand for shared pointer type.
		typedef boost::shared_ptr<PhysicsEntity> P;
		/// Local shorthand for weak pointer type.
		typedef boost::weak_ptr<PhysicsEntity> W;

		/// Factory method for instantiation. Initializes the model transformation to indentity.
		/// @param shadedMesh the ShadedMesh which the Entity uses for displaying itself.
		/// @return shared pointer referencing created instance
		static PhysicsEntity::P create(physx::PxScene* scene, Egg::Math::float3 position, Egg::Math::float4 orientation, Mesh::Multi::P multiMesh)
		{
			return PhysicsEntity::P(new PhysicsEntity(scene, position, orientation, multiMesh));
		}

		virtual ~PhysicsEntity();

		/// Returns the model matrix. To be used for rendering, and positioning of light sources and cameras attached to the entity.
		virtual Egg::Math::float4x4 getModelMatrix();

		/// Returns the inverse of the model matrix.
		virtual Egg::Math::float4x4 getModelMatrixInverse();

		/// Returns reference point in world space.
		virtual Egg::Math::float3 getPosition();

		/// Returns orientation as a quaternion in world space.
		virtual Egg::Math::float4 getOrientation();

		/// Returns the rotation matrix.
		virtual Egg::Math::float4x4 getRotationMatrix();

		/// Returns the inverse of the rotation matrix. To be used for the view transformation of attached cameras.
		virtual Egg::Math::float4x4 getRotationMatrixInverse();

		physx::PxRigidActor* getActor(){return actor;}

		physx::PxRigidDynamic* makeActorDynamic(physx::PxScene* scene);

		virtual void kill();
	};

}}
