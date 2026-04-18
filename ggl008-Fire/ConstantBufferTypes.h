#pragma once

#include <Egg/Math/float4x4.h>

using namespace Egg::Math;

__declspec(align(16)) struct PerObjectCb {
	float4x4 modelTransform;
	float4x4 modelTransformInverse;
};

__declspec(align(16)) struct PerFrameCb {
	float4x4 viewProjTransform;
	float4x4 rayDirTransform;
	float4 cameraPos;
	float4 lightPos;
	float4 lightPowerDensity;
	float4 billboardSize;
};
