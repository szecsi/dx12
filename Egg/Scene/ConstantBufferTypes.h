#pragma once
#include "../Common.h"
#include "Egg/Math/math.h"
#include "PerObjectData.h"

__declspec(align(16)) struct PerObjectCb {
	Egg::Scene::PerObjectData objects[1024];
};

__declspec(align(16)) struct PerFrameCb {
	Egg::Math::float4x4 viewProjTransform;
	Egg::Math::float4x4 rayDirTransform;
	Egg::Math::float4 cameraPos;
	Egg::Math::float4 lightPos;
	Egg::Math::float4 lightPowerDensity;
	Egg::Math::float4x4 lightViewProjTransform;
	Egg::Math::float4 lightPos2;
	Egg::Math::float4 lightPowerDensity2;
	Egg::Math::float4x4 lightViewProjTransform2;
	Egg::Math::float4 billboardSize;
	Egg::Math::float4 ahead;
	Egg::Math::float4 time;
};