#pragma once
#include "Scene/RigidBody.h"

namespace Egg11 { namespace Scene
{
	/// Base class for game objects
	GG_SUBCLASS(FixedRigidBody, RigidBody)
		/// Model transformation which describes entity position and orientation.
		Egg11::Math::float4x4 modelMatrix;

	/// Quaternion which describes entity orientation.
	Egg11::Math::float4 orientation;

	public:
		/// Returns the model matrix. To be used for rendering, and positioning of light sources and cameras attached to the entity.
		virtual Egg11::Math::float4x4 getModelMatrix() {
			return modelMatrix;
		}

		/// Returns the inverse of the model matrix. To be used for rendering.
		virtual Egg11::Math::float4x4 getModelMatrixInverse() {
			using namespace Egg11::Math;
			return modelMatrix.invert();
		}


		/// Returns reference point in world space.
		virtual Egg11::Math::float3 getPosition() {
			using namespace Egg11::Math;
			return (float4::wUnit * modelMatrix).xyz;
		}

		/// Returns the orientation as a quaternion.
		virtual Egg11::Math::float4 getOrientation() {
			return orientation;
		}

		/// Returns the rotation matrix.
		virtual Egg11::Math::float4x4 getRotationMatrix() {
			using namespace Egg11::Math;
			return modelMatrix * float4x4::translation(-getPosition());
		}

		/// Returns the inverse of the rotation matrix. To be used for the view transformation of attached cameras.
		virtual Egg11::Math::float4x4 getRotationMatrixInverse() {
			return getRotationMatrix().transpose();
		}

		/// Appends a translation to the model transformation.
		/// @param offset the translation vector
		void translate(const Egg11::Math::float3& offset)
		{
			using namespace Egg11::Math;
			modelMatrix *= float4x4::translation(offset);
		}

		/// Appends a rotation to the model transformation.
		/// @param axis the rotation axis vector
		/// @param angle the rotation angle
		void rotate(const Egg11::Math::float3& axis, float angle)
		{
			using namespace Egg11::Math;
			modelMatrix *= float4x4::rotation(axis, angle);
			float4 q = float4::quatAxisAngle(axis, angle);
			orientation = orientation.quatMul(q);
		}

		/// Updates time-varying entity properties. Returns whether the entity should be kept.
		/// @param dt time step in seconds
		virtual bool animate(float dt, float t){return true;}

	GG_ENDCLASS
}}
