#include "Entity.h"
#include "ControlState.h"

using namespace Egg;
using namespace Egg::Scene;

Entity::Entity(Egg::Mesh::Multi::P mesh, Egg::Scene::RigidBody::P rigidBody)
	:multiMesh(mesh), rigidBody(rigidBody)
{
}

void Entity::SetControlState(Egg::Scene::ControlStateP controlState){
	this->controlState = controlState;
}
