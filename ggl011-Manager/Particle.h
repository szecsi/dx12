#pragma once

#include "Egg/Math/math.h"
class Particle
{
public:
	Egg::Math::float3 position;
	Egg::Math::float3 velocity;
	float lifespan;
	float age;

	void reborn() {
		using namespace Egg::Math;
		position =
			float3::Random(-1, 1);
		velocity = position * 5;
		age = 0;
		lifespan = float1::Random(2, 5).x;
	}
	Particle() { reborn(); }

	void move(float dt)
	{
		position += velocity * dt;
		age += dt;
		if (age > lifespan)
			reborn();
	}


};
