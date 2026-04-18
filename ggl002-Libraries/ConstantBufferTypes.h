#pragma once

#include <Egg/Math/float4x4.h>

using Egg::Math::float4x4;

__declspec(align(16)) struct PerObjectCb {
	float4x4 modelTransform;
};
