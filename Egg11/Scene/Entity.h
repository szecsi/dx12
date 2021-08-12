#pragma once
#include "Mesh/Multi.h"
#include "Cam/Base.h"
#include "Scene/RigidBody.h"
#include "Scene/RenderParameters.h"

namespace Egg11 {
	namespace Scene
	{
		GG_CLASS(Entity)
			Egg11::Mesh::Multi::P mesh;
		Egg11::Scene::RigidBody::P rigidBody;

	protected:
		Entity(Egg11::Mesh::Multi::P mesh, Egg11::Scene::RigidBody::P rigidBody);
	public:

		void render(const Egg11::Scene::RenderParameters& params);
		void animate(float dt, float t);

	GG_ENDCLASS
}}
