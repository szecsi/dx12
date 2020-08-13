#pragma once

#include <Egg/Math/Float4x4.h>

using namespace Egg::Math;

__declspec(align(16)) struct PerObjectCb {
	Float4x4 model;
	Float4x4 invModel;
};

__declspec(align(16)) struct PerFrameCb {
	Float4x4 viewProj;
	Float4 lightPos;
	Float4 eyePos;
	Float4 lightIntensity;
	Float4x4 invViewProj;
};
