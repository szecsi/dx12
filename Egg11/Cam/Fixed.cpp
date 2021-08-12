#include "DXUT.h"
#include "Fixed.h"

using namespace Egg::Cam;
using namespace Egg::Scene;
using namespace Egg::Math;

const float3& Fixed::getEyePosition()
{
	float4x4 entityModelMatrix;
	Entity::P lockedOwner = owner.lock();
	if(!lockedOwner)
		entityModelMatrix = float4x4::identity;
	else
		entityModelMatrix = lockedOwner->getModelMatrix();
	
	return (eyePosition.xyz1 *  entityModelMatrix).xyz;
}

const float3& Fixed::getAhead()
{
	float4x4 entityRotationMatrix;
	Entity::P lockedOwner = owner.lock();
	if(!lockedOwner)
		entityRotationMatrix = float4x4::identity;
	else
		entityRotationMatrix = lockedOwner->getRotationMatrix();
	
	return (ahead.xyz0 * entityRotationMatrix).xyz;
}

const float4x4& Fixed::getViewDirMatrix()
{
	
	float4x4 entityRotationMatrixInverse;

	Entity::P lockedOwner = owner.lock();
	if(!lockedOwner)
		entityRotationMatrixInverse = float4x4::identity;
	else
		entityRotationMatrixInverse =  lockedOwner->getRotationMatrixInverse();

	float4x4 eyePosTranslationMatrix = float4x4::translation(eyePosition);

	//return (projMatrix).invert();
	viewDirMatrix = (entityRotationMatrixInverse * eyePosTranslationMatrix * viewMatrix  * projMatrix).invert();

	return viewDirMatrix;
}

const float4x4& Fixed::getViewMatrix()
{
	float4x4 entityModelMatrixInverse;
	Entity::P lockedOwner = owner.lock();
	if(!lockedOwner)
		entityModelMatrixInverse = float4x4::identity;
	else
		entityModelMatrixInverse = lockedOwner->getModelMatrixInverse();

	worldViewMatrix = entityModelMatrixInverse * viewMatrix;

	return worldViewMatrix;
}

const float4x4& Fixed::getProjMatrix() 
{
	return projMatrix;
}

Fixed::Fixed(boost::weak_ptr<Entity> owner)
{
	this->owner = owner;

	this->eyePosition = float3(0, 0, 0);
	this->ahead = float3(0, 0, 1);
	this->up  = float3(0, 1, 0);
	viewMatrix = float4x4::view(eyePosition, ahead, up);

	this->fov = 1.57;
	this->aspect = 1.33;
	this->front = 0.1f;
	this->back = 1000;
	projMatrix = float4x4::proj(fov, aspect, front, back);
}

Fixed::Fixed(boost::weak_ptr<Entity> owner, const float3& eyePosition, const float3& ahead, const float3& up)
{
	this->owner = owner;
	this->eyePosition = eyePosition;
	this->ahead = ahead;
	this->up = up;
	viewMatrix = float4x4::view(eyePosition, ahead, up);

	this->fov = 1.58;
	this->aspect = 1;
	this->front = 0.1f;
	this->back = 1000;
	projMatrix = float4x4::proj(fov, aspect, front, back);
}

Fixed::Fixed(boost::weak_ptr<Entity> owner, const float3& eyePosition, const float3& lookAt, const float3& up, double fov, double aspect, double front, double back)
{
	this->owner = owner;
	this->eyePosition = eyePosition;
	this->ahead = ahead;
	this->up = up;
	viewMatrix = float4x4::view(eyePosition, ahead, up);
	
	this->fov = fov;
	this->aspect = aspect;
	this->front = front;
	this->back = back;

	projMatrix = float4x4::proj(fov, aspect, front, back);
}

void Fixed::setFov(float fov)
{
	this->fov = fov;
	projMatrix = float4x4::proj(fov, aspect, front, back);
}


void Fixed::setPlanes(float front, float back)
{
	this->front = front;
	this->back = back;
	projMatrix = float4x4::proj(fov, aspect, front, back);
}

void Fixed::setAspect(float aspect)
{
	this->aspect = aspect;
	projMatrix = float4x4::proj(fov, aspect, front, back);
}
