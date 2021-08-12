#pragma once
#include "Egg11/Math/math.h"


class DualQuaternion
{
public:
	Egg11::Math::float4 orientation;
	Egg11::Math::float4 translation;

	DualQuaternion(){}
	DualQuaternion(const Egg11::Math::float4& orientation, const Egg11::Math::float4& translation);

	void set(const Egg11::Math::float4& orientation, const Egg11::Math::float4& translation);

	DualQuaternion operator*(const DualQuaternion& other) const;
};
