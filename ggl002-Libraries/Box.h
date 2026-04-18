#pragma once

#include <Egg/Common.h>
#include <Egg/Mesh/Shaded.h>
#include <Egg/Mesh/Prefabs.h>
#include <Egg/ConstantBuffer.hpp>
#include "ConstantBufferTypes.h"
#include <Egg/Math/Math.h>

using namespace Egg::Math;

GG_CLASS(Box)
public:
	Egg::Mesh::Shaded::P shadedMesh;

	Egg::ConstantBuffer<PerObjectCb> constantBuffer;

	float4x4 scale;
	float4x4 rotation;
	float4x4 translation;
	float4x4 meshTransform;

	float3 position;
	float3 speed;
	float3 gravity;

	Box(Egg::Mesh::Shaded::P shadedm) : shadedMesh{ shadedm }, constantBuffer{}, scale{}, rotation{}, translation{}, meshTransform{} {
		gravity = float3{ 0.0f, -0.931f, 0.0f };
	}

	void Update(float dt, float T) {
		speed += gravity * dt;
		position += speed * dt;

		if(position.y < -1.0f) {
			speed = -speed;
			position.y = -1.0f;
		}

		translation = float4x4::Translation(position);

		constantBuffer->modelTransform = meshTransform * scale * rotation * translation;
		constantBuffer.Upload();
	}

	void Draw(ID3D12GraphicsCommandList * commandList) {
		shadedMesh->GetMaterial()->SetConstantBuffer(cons);
		shadedMesh->Draw(commandList);
	}

	void CreateResources(ID3D12Device * device) {
		constantBuffer.CreateResources(device);
	}

GG_ENDCLASS
