#pragma once
#include "Math/math.h"
#include "PxPhysicsAPI.h"

namespace Egg { namespace Physics
{

	inline const physx::PxVec3& operator~(const Egg::Math::float3& v)
	{
		return *(const physx::PxVec3*)&v;
	}
	inline physx::PxVec3& operator~(Egg::Math::float3& v)
	{
		return *(physx::PxVec3*)&v;
	}

	inline const physx::PxQuat& operator~(const Egg::Math::float4& v)
	{
		return *(const physx::PxQuat*)&v;
	}
	inline physx::PxQuat& operator~(Egg::Math::float4& v)
	{
		return *(physx::PxQuat*)&v;
	}

	inline const Egg::Math::float3& operator~(const physx::PxVec3& v)
	{
		return *(const Egg::Math::float3*)&v;
	}
	inline Egg::Math::float3& operator~(physx::PxVec3& v)
	{
		return *(Egg::Math::float3*)&v;
	}

	inline const Egg::Math::float4& operator~(const physx::PxQuat& v)
	{
		return *(const Egg::Math::float4*)&v;
	}
	inline Egg::Math::float4& operator~(physx::PxQuat& v)
	{
		return *(Egg::Math::float4*)&v;
	}

}}
