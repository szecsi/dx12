#pragma once

#include <Egg/Math/float4x4.h>

using namespace Egg::Math;

__declspec(align(16)) struct PerObjectCb {
	float4x4 model;
	float4x4 invModel;
};

__declspec(align(16)) struct PerFrameCb {
	float4x4 viewProj;
	float4 lightPos;
	float4 eyePos;
	float4 lightIntensity;
	float4x4 invViewProj;
};
