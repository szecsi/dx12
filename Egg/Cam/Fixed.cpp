#include "Fixed.h"

using namespace Egg::Cam;
using namespace Egg::Scene;
using namespace Egg::Math;

const Float3& Fixed::GetEyePosition()
{
	Float4x4 entityModelMatrix;
	Entity::P lockedOwner = owner.lock();
	if(!lockedOwner)
		entityModelMatrix = Float4x4::Identity;
	else
		entityModelMatrix = lockedOwner->GetRigidBody()->GetModelMatrix();
	
	return (Float4(eyePosition, 1) * entityModelMatrix).xyz;
}

const Float3& Fixed::GetAhead()
{
	Float4x4 entityRotationMatrix;
	Entity::P lockedOwner = owner.lock();
	if(!lockedOwner)
		entityRotationMatrix = Float4x4::Identity;
	else
		entityRotationMatrix = lockedOwner->GetRigidBody()->GetRotationMatrix();
	
	return (Float4(ahead, 0) * entityRotationMatrix).xyz;
}

const Float4x4& Fixed::GetRayDirMatrix()
{
	Float4x4 entityRotationMatrixInverse;

	Entity::P lockedOwner = owner.lock();
	if(!lockedOwner)
		entityRotationMatrixInverse = Float4x4::Identity;
	else
		entityRotationMatrixInverse = lockedOwner->GetRigidBody()->GetRotationMatrixInverse();

	Float4x4 eyePosTranslationMatrix = Float4x4::Translation(eyePosition);

//	return (projMatrix).Invert();
	rayDirMatrix = (entityRotationMatrixInverse * eyePosTranslationMatrix * viewMatrix  * projMatrix).Invert();
	return rayDirMatrix;
}

const Float4x4& Fixed::GetViewMatrix()
{
	static Float4x4 entityModelMatrixInverse;
	Entity::P lockedOwner = owner.lock();
	if(!lockedOwner)
		entityModelMatrixInverse = Float4x4::Identity;
	else
		entityModelMatrixInverse = lockedOwner->GetRigidBody()->GetModelMatrixInverse();

	viewMatrixWorld = entityModelMatrixInverse * viewMatrix;
	return viewMatrixWorld;
}

const Float4x4& Fixed::GetProjMatrix() 
{
	return projMatrix;
}

Fixed::Fixed(Entity::W owner)
{
	this->owner = owner;

	this->eyePosition = Float3(0, 0, 0);
	this->ahead = Float3(0, 0, 1);
	this->up  = Float3(0, 1, 0);
	viewMatrix = Float4x4::View(eyePosition, ahead, up);

	this->fov = 1.57;
	this->aspect = 1.33;
	this->front = 0.1;
	this->back = 1000;
	projMatrix = Float4x4::Proj(fov, aspect, front, back);
}

Fixed::Fixed(Entity::W owner, const Float3& eyePosition, const Float3& ahead, const Float3& up)
{
	this->owner = owner;
	this->eyePosition = eyePosition;
	this->ahead = ahead;
	this->up = up;
	viewMatrix = Float4x4::View(eyePosition, ahead, up);

	this->fov = 1.58;
	this->aspect = 1;
	this->front = 1;
	this->back = 1000;
	projMatrix = Float4x4::Proj(fov, aspect, front, back);
}

Fixed::Fixed(Entity::W owner, const Float3& eyePosition, const Float3& ahead, const Float3& up, double fov, double aspect, double front, double back)
{
	this->owner = owner;
	this->eyePosition = eyePosition;
	this->ahead = ahead;
	this->up = up;
	viewMatrix = Float4x4::View(eyePosition, ahead, up);
	
	this->fov = fov;
	this->aspect = aspect;
	this->front = front;
	this->back = back;

	projMatrix = Float4x4::Proj(fov, aspect, front, back);
}

void Fixed::SetAspect(float aspect)
{
	this->aspect = aspect;
	projMatrix = Float4x4::Proj(fov, aspect, front, back);
}
