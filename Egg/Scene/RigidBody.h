#pragma once
#include "Egg/Math/math.h"

namespace Egg { namespace Scene
{
	/// Base class for game objects
	GG_CLASS(RigidBody)
	public:
		/// Returns the model matrix. To be used for rendering, and positioning of light sources and cameras attached to the entity.
		virtual Egg::Math::Float4x4 GetModelMatrix()=0;

		/// Returns the inverse of the model matrix. To be used for rendering.
		virtual Egg::Math::Float4x4 GetModelMatrixInverse()=0;

		/// Returns reference point in world space.
		virtual Egg::Math::Float3 GetPosition()=0;

		/// Returns the orientation as a quaternion.
		virtual Egg::Math::Float4 GetOrientation()=0;

		/// Returns the rotation matrix.
		virtual Egg::Math::Float4x4 GetRotationMatrix()=0;

		/// Returns the inverse of the rotation matrix. To be used for the view transformation of attached cameras.
		virtual Egg::Math::Float4x4 GetRotationMatrixInverse()=0;

		/// Updates time-varying entity properties. Returns whether the entity should be kept.
		/// @param dt time step in seconds
		virtual bool Update(float dt, float t){return true;}

		virtual void AddForce(Egg::Math::Float3 force) {}
		virtual void AddTorque(Egg::Math::Float3 torque) {}
	GG_ENDCLASS
}}
