#include "../Common.h"
#include "PhysicsRigidBody.h"
#include "PhysicsMathConversions.h"

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

PhysicsRigidBody::PhysicsRigidBody(
	physx::PxScene* scene,
	Model::P model,
	Egg::Math::Float3 position,
	Egg::Math::Float4 orientation){
		actor = scene->getPhysics().
			createRigidDynamic(
			PxTransform(~position, ~orientation ));

		for (auto shape : model->shapes) {
			actor->attachShape(*shape);
		}

		actor->setActorFlags(model->actorFlags);
		actor->setRigidBodyFlags(model->rigidBodyFlags);
		PxRigidBodyExt::updateMassAndInertia(*actor, model->density);

		scene->addActor(*actor);
}

PhysicsRigidBody::~PhysicsRigidBody()
{
//	actor->userData = NULL;
//	PxScene* scene = actor->getScene();
//	scene->removeActor(*actor);
//	actor->release();
}

Math::Float4x4 PhysicsRigidBody::GetModelMatrix()
{
	using namespace Egg::Math;

	PxTransform m = actor->getGlobalPose();

	float angle;
	Float3 axis;
	toRadiansAndUnitAxis(m.q, angle, ~axis);

	return Float4x4::Rotation(axis, angle) * Float4x4::Translation(~m.p);
}

Math::Float4x4 PhysicsRigidBody::GetModelMatrixInverse()
{
	using namespace Egg::Math;

	PxTransform m = actor->getGlobalPose();

	float angle;
	Float3 axis;
	toRadiansAndUnitAxis(m.q, angle, ~axis);

	return Float4x4::Translation(~-m.p) * Float4x4::Rotation(axis, -angle);
}

Math::Float3 PhysicsRigidBody::GetPosition()
{
	using namespace Egg::Math;

	PxTransform m = actor->getGlobalPose();
	return ~m.p;
}

Math::Float4 PhysicsRigidBody::GetOrientation()
{
	using namespace Egg::Math;
	PxTransform m = actor->getGlobalPose();
	return ~m.q;
}

Math::Float4x4 PhysicsRigidBody::GetRotationMatrix()
{
	using namespace Egg::Math;
	PxTransform m = actor->getGlobalPose();

	float angle;
	Float3 axis;
	toRadiansAndUnitAxis(m.q, angle, ~axis);

	return Float4x4::Rotation(axis, angle);
}

Math::Float4x4 PhysicsRigidBody::GetRotationMatrixInverse()
{
	using namespace Egg::Math;
	PxTransform m = actor->getGlobalPose();

	float angle;
	Float3 axis;
	toRadiansAndUnitAxis(m.q, angle, ~axis);

	return Float4x4::Rotation(axis, -angle);
}


void PhysicsRigidBody::AddForce(Egg::Math::Float3 force) {
	actor->addForce(~force);
}

void PhysicsRigidBody::AddTorque(Egg::Math::Float3 torque) {
	actor->addTorque(~torque);
}
