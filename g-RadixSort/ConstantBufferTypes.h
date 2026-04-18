#pragma once

#include <Egg/Math/float4x4.h>

using namespace Egg11::Math;

__declspec(align(16)) struct PerObjectCb {
	float4x4 modelTransform;
};
