#include "Fixed.h"

using namespace Egg;
using namespace Egg::Math;

Cam::Fixed::Fixed(Egg::Scene::Entity::P owner,
	Float3 position,
	Float3 ahead,
	Float3 up,
	float fov,
	float aspect,
	float front,
	float back):
	owner(owner),
	position(position),
	ahead(ahead),
	modelUp(up),
	fov(fov),
	aspect(aspect),
	nearPlane(front),
	farPlane(back)
{
/*	position = Float3::UnitZ * -10.0;
	ahead = Float3::UnitZ;
	right = Float3::UnitX;
	yaw = 0.0;
	pitch = 0.0;

	fov = 1.57f;
	nearPlane = 0.1f;
	farPlane = 1000.0f;*/
	SetAspect(1.33f);

	viewMatrix = Float4x4::View(position, ahead, modelUp);
	rayDirMatrix = (Float4x4::View(Float3::Zero, ahead, modelUp) * projMatrix).Invert();
	
}

Cam::Fixed::P Cam::Fixed::SetView(Egg::Math::Float3 position, Egg::Math::Float3 ahead)
{
	this->position = position;
	this->ahead =    ahead;
	UpdateView();
	return GetShared();
}

Cam::Fixed::P Cam::Fixed::SetProj(float fov, float aspect, float nearPlane, float farPlane)
{
	this->fov = fov;
	this->aspect = aspect;
	this->nearPlane = nearPlane;
	this->farPlane = farPlane;
	return GetShared();
}


const Float3& Cam::Fixed::GetEyePosition()
{
	Egg::Scene::Entity::P entity = owner.lock();
	if (entity) {
		return (Float4(position, 1) * entity->GetRigidBody()->GetModelMatrix()).xyz;
	}
	return position;
}

const Float3& Cam::Fixed::GetAhead()
{
	Egg::Scene::Entity::P entity = owner.lock();
	//if (entity) {
	//	return (Float4(ahead, 0) * entity->GetRigidBody()->GetRotationMatrix()).xyz;
	//}
	return ahead;
}

const Float4x4& Cam::Fixed::GetRayDirMatrix()
{
	Egg::Scene::Entity::P entity = owner.lock();
	if (entity) {
		auto em = entity->GetRigidBody()->GetRotationMatrix();
		rayDirMatrixWorld = rayDirMatrix * em;
	}
	else {
		rayDirMatrixWorld = rayDirMatrix;
	}
	return rayDirMatrixWorld;
}

const Float4x4& Cam::Fixed::GetViewMatrix()
{
	Egg::Scene::Entity::P entity = owner.lock();
	if (entity) {
		auto em = entity->GetRigidBody()->GetModelMatrixInverse();
		viewMatrixWorld = em * viewMatrix;
	}
	else {
		viewMatrixWorld = viewMatrix;
	}
	return viewMatrixWorld;
}

const Float4x4& Cam::Fixed::GetProjMatrix()
{
	return projMatrix;
}

void Cam::Fixed::UpdateView()
{
	viewMatrix = Float4x4::View(position, ahead, modelUp);
	rayDirMatrix = (Float4x4::View(Float3::Zero, GetAhead(), modelUp) * projMatrix).Invert();

	right = modelUp.Cross(ahead).Normalize();
	yaw = atan2f( ahead.x, ahead.z );
	pitch = -atan2f( ahead.y, ahead.xz.Length() );
}

void Cam::Fixed::UpdateProj()
{
	projMatrix = Float4x4::Proj(fov, aspect, nearPlane, farPlane);
}

void Cam::Fixed::SetAspect(float aspect)
{
	this->aspect = aspect;
	UpdateProj();
}