#pragma once

#include <Egg/Math/Float4x4.h>

using namespace Egg::Math;

__declspec(align(16)) struct PerObjectCb {
	Float4x4 modelTransform;
};
