#include "DXUT.h"
#include "Physics/PhysicsEntity.h"
#include "Physics/PhysicsMathConversions.h"

using namespace Egg;
using namespace Egg::Physics;
using namespace physx;

	void toRadiansAndUnitAxis(PxQuat& q, PxReal& angle, PxVec3& axis)
	{
		const PxReal quatEpsilon = (PxReal(1.0e-8f));
		const PxReal s2 = q.x*q.x+q.y*q.y+q.z*q.z;
		if(s2<quatEpsilon*quatEpsilon)  // can't extract a sensible axis
		{
			angle = 0;
			axis = PxVec3(1,0,0);
		}
		else
		{
			const PxReal s = PxRecipSqrt(s2);
			axis = PxVec3(q.x,q.y,q.z) * s; 
			angle = abs(q.w)<quatEpsilon ? PxPi : PxAtan2(s2*s, q.w) * 2;
		}

	}

PhysicsEntity::PhysicsEntity(physx::PxScene* scene, Egg::Math::float3 position, Egg::Math::float4 orientation, Mesh::Multi::P multiMesh)
	:Scene::Entity(multiMesh)
{
	actor = scene->getPhysics().
		createRigidStatic(
		PxTransform(~position, ~orientation ));
}

PhysicsEntity::~PhysicsEntity()
{
//	actor->userData = NULL;
//	PxScene* scene = actor->getScene();
//	scene->removeActor(*actor);
//	actor->release();
}

Math::float4x4 PhysicsEntity::getModelMatrix()
{
	using namespace Egg::Math;

	PxTransform m = actor->getGlobalPose();

	float angle;
	float3 axis;
	toRadiansAndUnitAxis(m.q, angle, ~axis);

	return float4x4::rotation(axis, angle) * float4x4::translation(~m.p);
}

Math::float4x4 PhysicsEntity::getModelMatrixInverse()
{
	using namespace Egg::Math;

	PxTransform m = actor->getGlobalPose();

	float angle;
	float3 axis;
	toRadiansAndUnitAxis(m.q, angle, ~axis);

	return float4x4::translation(~-m.p) * float4x4::rotation(axis, -angle);
}

Math::float3 PhysicsEntity::getPosition()
{
	using namespace Egg::Math;

	PxTransform m = actor->getGlobalPose();
	return ~m.p;
}

Math::float4 PhysicsEntity::getOrientation()
{
	using namespace Egg::Math;
	PxTransform m = actor->getGlobalPose();
	return ~m.q;
}

Math::float4x4 PhysicsEntity::getRotationMatrix()
{
	using namespace Egg::Math;
	PxTransform m = actor->getGlobalPose();

	float angle;
	float3 axis;
	toRadiansAndUnitAxis(m.q, angle, ~axis);

	return float4x4::rotation(axis, angle);
}

Math::float4x4 PhysicsEntity::getRotationMatrixInverse()
{
	using namespace Egg::Math;
	PxTransform m = actor->getGlobalPose();

	float angle;
	float3 axis;
	toRadiansAndUnitAxis(m.q, angle, ~axis);

	return float4x4::rotation(axis, -angle);
}

PxRigidDynamic* PhysicsEntity::makeActorDynamic(PxScene* scene)
{
	PxTransform pose = actor->getGlobalPose();
	PxRigidDynamic*	rigidDynamic = scene->getPhysics().createRigidDynamic( pose );
	unsigned int nShapes = actor->getNbShapes();
	PxShape** shapes = new PxShape*[nShapes];
	actor->getShapes(shapes, nShapes);
	for(int iShape=0; iShape<nShapes; iShape++)
	{
		PxMaterial* material;
		shapes[iShape]->getMaterials(&material, 1);
		rigidDynamic->createShape(shapes[iShape]->getGeometry().any(), *material, shapes[iShape]->getLocalPose());
	}
	delete shapes;
	actor->release();
	actor = rigidDynamic;
	return rigidDynamic;
}

void PhysicsEntity::kill()
{
	__super::kill();
	actor->release();
	actor = NULL;
}