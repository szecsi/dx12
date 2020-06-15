#pragma once

#include "../Common.h"
#include "Egg/Math/math.h"

namespace Egg {
	namespace Scene {
		__declspec(align(16)) struct PerObjectData {
			Egg::Math::Float4x4 modelTransform;
			Egg::Math::Float4x4 modelTransformInverse;
			Egg::Math::Float4x4 paddingTo256Bytes0;
			Egg::Math::Float4x4 paddingTo256Bytes1;
		};
	}
}