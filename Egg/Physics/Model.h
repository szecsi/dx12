#pragma once
#include "../Common.h"
#include "physx/PxPhysicsAPI.h"
#include <vector>

namespace Egg { namespace Physics
{
	GG_CLASS(Model)
		friend class PhysicsRigidBody;
		std::vector<physx::PxShape*> shapes;

	protected:
		Model() {
			density = 1.0f;
		}
	public:
		float density;
		physx::PxActorFlags actorFlags;
		physx::PxRigidBodyFlags rigidBodyFlags;

		void addShape(physx::PxShape* shape) {
			shapes.push_back(shape);
		}
	GG_ENDCLASS

}}
