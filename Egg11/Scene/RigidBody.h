#pragma once
#include "Math/math.h"

namespace Egg11 { namespace Scene
{
	/// Base class for game objects
	GG_CLASS(RigidBody)
	public:
		/// Returns the model matrix. To be used for rendering, and positioning of light sources and cameras attached to the entity.
		virtual Egg11::Math::float4x4 getModelMatrix()=0;

		/// Returns the inverse of the model matrix. To be used for rendering.
		virtual Egg11::Math::float4x4 getModelMatrixInverse()=0;

		/// Returns reference point in world space.
		virtual Egg11::Math::float3 getPosition()=0;

		/// Returns the orientation as a quaternion.
		virtual Egg11::Math::float4 getOrientation()=0;

		/// Returns the rotation matrix.
		virtual Egg11::Math::float4x4 getRotationMatrix()=0;

		/// Returns the inverse of the rotation matrix. To be used for the view transformation of attached cameras.
		virtual Egg11::Math::float4x4 getRotationMatrixInverse()=0;

		/// Updates time-varying entity properties. Returns whether the entity should be kept.
		/// @param dt time step in seconds
		virtual bool animate(float dt, float t){return true;}

	GG_ENDCLASS
}}
