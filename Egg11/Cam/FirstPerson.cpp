#include "stdafx.h"
#include "Cam/FirstPerson.h"

using namespace Egg11;
using namespace Egg11::Math;

Cam::FirstPerson::FirstPerson()
{
	//position = float3::zero;
	position = float3(-0.6, 0.5, -0.6);
	ahead = float3::zUnit;
	right = float3::xUnit;
	//yaw = 0.0;
	//pitch = 0.0;
	yaw = 0.56;
	pitch = 0.48;

	fov = 1.57f;
	nearPlane = 0.1f;
	farPlane = 50.0f;
	setAspect(1.33f);
	//setAspect(1.0f);

	viewMatrix = float4x4::view(position, ahead, float3::yUnit);
	viewDirMatrix = (float4x4::view(float3::zero, ahead, float3::yUnit) * projMatrix).invert();

	speed = 0.5f;

	lastMousePos = int2::zero;
	mouseDelta = float2::zero;

	wPressed = false;
	aPressed = false;
	sPressed = false;
	dPressed = false;
	qPressed = false;
	ePressed = false;
}

Cam::FirstPerson::P Cam::FirstPerson::setView(Egg11::Math::float3 position, Egg11::Math::float3 ahead)
{
	this->position = position;
	this->ahead =    ahead;
	updateView();
	return Egg11::Cam::FirstPerson::getShared();
}

Cam::FirstPerson::P Cam::FirstPerson::setProj(float fov, float aspect, float nearPlane, float farPlane)
{
	this->fov = fov;
	this->aspect = aspect;
	this->nearPlane = nearPlane;
	this->farPlane = farPlane;
	return getShared();
}

Cam::FirstPerson::P Cam::FirstPerson::setSpeed(float speed)
{
	this->speed = speed;
	return getShared();
}

const float3& Cam::FirstPerson::getEyePosition()
{
	return position;
}

const float3& Cam::FirstPerson::getAhead()
{
	return ahead;
}

const float4x4& Cam::FirstPerson::getViewDirMatrix()
{
	return viewDirMatrix;
}

const float4x4& Cam::FirstPerson::getViewMatrix()
{
	return viewMatrix;
}

const float4x4& Cam::FirstPerson::getProjMatrix()
{
	return projMatrix;
}

Egg11::Math::float4x4 Cam::FirstPerson::getOrthoProjMatrix(float worldWidth, float wolrdHeight)
{
	return float4x4(
		2.0/worldWidth, 0.0f, 0.0f, 0.0f,
		0.0f, 2.0/wolrdHeight, 0.0f, 0.0f,
		0.0f, 0.0f, -2/(farPlane - nearPlane), 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f);
}

void Cam::FirstPerson::updateView()
{
	viewMatrix = float4x4::view(position, ahead, float3::yUnit);
	viewDirMatrix = (float4x4::view(float3::zero, ahead, float3::yUnit) * projMatrix).invert();

	right = float3::yUnit.cross(ahead).normalize();
	yaw = atan2f( ahead.x, ahead.z );
	pitch = -atan2f( ahead.y, ahead.xz.length() );
}

void Cam::FirstPerson::updateProj()
{
	projMatrix = float4x4::proj(fov, aspect, nearPlane, farPlane);
}

void Cam::FirstPerson::animate(double dt)
{
	if(wPressed)
		position += ahead * (shiftPressed?speed*5.0:speed) * dt;
	if(sPressed)
		position -= ahead * (shiftPressed?speed*5.0:speed) * dt;
	if(aPressed)
		position -= right * (shiftPressed?speed*5.0:speed) * dt;
	if(dPressed)
		position += right * (shiftPressed?speed*5.0:speed) * dt;
	if(qPressed)
		position -= float3(0,1,0) * (shiftPressed?speed*5.0:speed) * dt;
	if(ePressed)
		position += float3(0,1,0) * (shiftPressed?speed*5.0:speed) * dt;

	yaw += mouseDelta.x * 0.02f;
	pitch += mouseDelta.y * 0.02f;
	pitch = float1(pitch).clamp(-3.14/2, +3.14/2);

	mouseDelta = float2::zero;

	ahead = float3(sin(yaw)*cos(pitch), -sin(pitch), cos(yaw)*cos(pitch) );

	updateView();
}

void Cam::FirstPerson::processMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if(uMsg == WM_KEYDOWN)
	{
		if(wParam == 'W')
			wPressed = true;
		else if(wParam == 'A')
			aPressed = true;
		else if(wParam == 'S')
			sPressed = true;
		else if(wParam == 'D')
			dPressed = true;
		else if(wParam == 'Q')
			qPressed = true;
		else if(wParam == 'E')
			ePressed = true;
		else if(wParam == VK_SHIFT)
			shiftPressed = true;
	}
	else if(uMsg == WM_KEYUP)
	{
		if(wParam == 'W')
			wPressed = false;
		else if(wParam == 'A')
			aPressed = false;
		else if(wParam == 'S')
			sPressed = false;
		else if(wParam == 'D')
			dPressed = false;
		else if(wParam == 'Q')
			qPressed = false;
		else if(wParam == 'E')
			ePressed = false;
		else if(wParam == VK_SHIFT)
			shiftPressed = false;
		else if(wParam == VK_ADD)
			speed *= 2;
		else if(wParam == VK_SUBTRACT)
			speed *= 0.5;
	}
	else if(uMsg == WM_KILLFOCUS)
	{
		wPressed = false;
		aPressed = false;
		sPressed = false;
		dPressed = false;
		qPressed = false;
		ePressed = false;
		shiftPressed = false;
	}
	else if(uMsg == WM_LBUTTONDOWN)
	{
		lastMousePos = int2( LOWORD(lParam), HIWORD(lParam));
	}
	else if(uMsg == WM_LBUTTONUP)
	{
		mouseDelta = float2::zero;
	}
	else if(uMsg == WM_MOUSEMOVE && (wParam & MK_LBUTTON))
	{
		int2 mousePos( LOWORD(lParam), HIWORD(lParam));
		mouseDelta = mousePos - lastMousePos;

		lastMousePos = mousePos;
	}
}

void Cam::FirstPerson::setAspect(float aspect)
{
	this->aspect = aspect;
	updateProj();
}