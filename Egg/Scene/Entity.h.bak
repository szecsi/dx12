#pragma once

#include "../Common.h"
#include "Egg/Mesh/Multi.h"
#include "Egg/Math/math.h"
#include "PerObjectData.h"

namespace Egg { 
	namespace Scene {

		/// Base class for game objects
		GG_CLASS( Entity )
		protected:
			/// A mesh complete with shading information for multiple rendering miens for multiple geometries (render component)
			Mesh::Multi::P multiMesh;

			/// Constructor
			/// @param multiMesh the mesh object responsible for rendering the entity
			Entity(Mesh::Multi::P multiMesh):multiMesh(multiMesh){}
		public:
			virtual ~Entity(){}

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
			/// Copies matrix data to constant buffer ???? TODO parameters
			/// @param dt time step in seconds
			virtual void Update(float dt, float t, PerObjectData& data) {
				data.modelTransform = GetModelMatrix();
				data.modelTransformInverse = GetModelMatrixInverse();
			};

			/// Renders the entity as seen from a given camera in a given mien.
			virtual void Draw(ID3D12GraphicsCommandList* commandList, unsigned int mien, unsigned int objectIndex) {
				multiMesh->Draw(commandList, mien, objectIndex);
			}

			// Removes entity-associated objects from all linked systems (e.g. Physics actor)
			virtual void Kill(){}
	};
}}
