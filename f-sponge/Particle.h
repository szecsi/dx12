#pragma once

#include <cstdint>

#include "Math/math.h"
class Particle
{
	friend class Game;
	Egg11::Math::float3 position;
	float massDensity;
	Egg11::Math::float3 velocity;
	float pressure;
	Egg11::Math::float3 temp;
	unsigned int zindex;

	//float lifespan;
	//float age;
public:
	void reborn() {
		using namespace Egg11::Math;
		position = float3::random(-1.0 * 0.0457, 1.0 * 0.0457);
		position.y += 0.05;
		position.y *= 5.0;
		//position.y += 5.0;

		//position = float3(0.0, 0.5, 0.0);
		massDensity = 0.0;
		velocity = float3 (0.0,0.0,0.0);
		pressure = 0.0;

		//age = 0;
		//lifespan = float1::random(2, 5);
	}
	Particle() { reborn(); }


};

class ControlParticle
{
	friend class Game;
	Egg11::Math::float3 position;
	float controlPressureRatio;
	Egg11::Math::float3	nonAnimatedPos;
	float	temp;
	Egg11::Math::float4	blendWeights;
	Egg11::Math::float4	blendIndices; // uint4
};