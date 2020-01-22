#pragma once
#include "../Common.h"
#include "RigidBody.h"

namespace Egg { namespace Scene
{
	/// Base class for game objects
	GG_SUBCLASS(FixedRigidBody, RigidBody)
		/// Model transformation which describes entity position and orientation.
		Egg::Math::Float4x4 modelMatrix;

		/// Quaternion which describes entity orientation.
		Egg::Math::Float4 orientation;

		public:
			/// Returns the model matrix. To be used for rendering, and positioning of light sources and cameras attached to the entity.
			virtual Egg::Math::Float4x4 GetModelMatrix() override {
				return modelMatrix;
			}

			/// Returns the inverse of the model matrix. To be used for rendering.
			virtual Egg::Math::Float4x4 GetModelMatrixInverse() override {
				using namespace Egg::Math;
				return modelMatrix.Invert();
			}


			/// Returns reference point in world space.
			virtual Egg::Math::Float3 GetPosition() override {
				using namespace Egg::Math;
				return (Float4::UnitW * modelMatrix).xyz;
			}

			/// Returns the orientation as a quaternion.
			virtual Egg::Math::Float4 GetOrientation() override {
				return orientation;
			}

			/// Returns the rotation matrix.
			virtual Egg::Math::Float4x4 GetRotationMatrix() override {
				using namespace Egg::Math;
				return modelMatrix * Float4x4::Translation(-GetPosition());
			}

			/// Returns the inverse of the rotation matrix. To be used for the view transformation of attached cameras.
			virtual Egg::Math::Float4x4 GetRotationMatrixInverse() override {
				return GetRotationMatrix().Transpose();
			}

			/// Appends a translation to the model transformation.
			/// @param offset the translation vector
			void Translate(const Egg::Math::Float3& offset)
			{
				using namespace Egg::Math;
				modelMatrix *= Float4x4::Translation(offset);
			}

			/// Appends a rotation to the model transformation.
			/// @param axis the rotation axis vector
			/// @param angle the rotation angle
			void Rotate(const Egg::Math::Float3& axis, float angle) {
				using namespace Egg::Math;
				modelMatrix *= Float4x4::Rotation(axis, angle);
				// quaternion from axis and angle
				Float4 o = Float4(axis.Normalize() * sinf(angle / 2), cosf(angle / 2));
				// quaternion multiplication
				orientation =
					Float4(
						orientation.y * o.z - orientation.z * o.y + orientation.w * o.x + orientation.x * o.w,
						orientation.z * o.x - orientation.x * o.z + orientation.w * o.y + orientation.y * o.w,
						orientation.x * o.y - orientation.y * o.x + orientation.w * o.z + orientation.z * o.w,
						orientation.w * o.w - (orientation.x * o.x + orientation.y * o.y + orientation.z * o.z));
			}

			/// Updates time-varying entity properties. Returns whether the entity should be kept.
			/// @param dt time step in seconds
			virtual bool Update(float dt, float t) override{
				return true;
			}

	GG_ENDCLASS
}}
