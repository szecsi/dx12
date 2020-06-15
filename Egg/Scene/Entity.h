#pragma once

#include "../Common.h"
#include "Egg/Mesh/Multi.h"
#include "Egg/Math/math.h"
#include "PerObjectData.h"
#include "RigidBody.h"
#include "ControlState.h"

namespace Egg {
	namespace Scene
	{
		GG_CLASS(Entity)
			Egg::Mesh::Multi::P multiMesh;
			Egg::Scene::RigidBody::P rigidBody;
			Egg::Scene::ControlStateP controlState;

		protected:
			Entity(Egg::Mesh::Multi::P mesh, Egg::Scene::RigidBody::P rigidBody);
		public:
			void SetControlState(Egg::Scene::ControlStateP controlState);

			Egg::Scene::RigidBody::P GetRigidBody() { return rigidBody; }
			Egg::Scene::ControlStateP GetControlState() { return controlState; }

			/// Updates time-varying entity properties. Returns whether the entity should be kept.
			/// Copies matrix data to constant buffer ???? TODO parameters
			/// @param dt time step in seconds
			virtual bool Update(float dt, float t, PerObjectData& data) {
				bool alive = true;
				if (controlState)
					alive = controlState->control(dt);
				rigidBody->Update(dt, t);
				data.modelTransform = rigidBody->GetModelMatrix();
				data.modelTransformInverse = rigidBody->GetModelMatrixInverse();
				return alive;
			};


			virtual void Draw(ID3D12GraphicsCommandList* commandList, unsigned int mien, unsigned int objectIndex) {
				multiMesh->Draw(commandList, mien, objectIndex);
			}

			void Animate(float dt, float t);

	GG_ENDCLASS
}}
