#pragma once
#include "../Math/Math.h"
#include "../Common.h"
#include "Base.h"
#include "../Scene/Entity.h"

namespace Egg { namespace Cam {
	//TODO: shiftpressed false, gyorsul dt/t, updateproj
	GG_SUBCLASS(Fixed, Cam::Base)
		Egg::Math::float3 position;
		Egg::Math::float3 ahead;
		Egg::Math::float3 right;
		Egg::Math::float3 modelUp;
		float yaw;
		float pitch;

		float fov;
		float aspect;
		float nearPlane;
		float farPlane;

		Egg::Math::float4x4 viewMatrix;
		Egg::Math::float4x4 projMatrix;
		Egg::Math::float4x4 rayDirMatrix;

		Egg::Math::float4x4 viewMatrixWorld;
		Egg::Math::float4x4 rayDirMatrixWorld;

		Egg::Scene::Entity::W owner;

		void UpdateView();
		void UpdateProj();

	protected:
		Fixed(Egg::Scene::Entity::P owner,
			Egg::Math::float3 position,
			Egg::Math::float3 ahead,
			Egg::Math::float3 up,
			float fov,
			float aspect,
			float front,
			float back);
	public:

		P SetView(Egg::Math::float3 position, Egg::Math::float3 ahead);
		P SetProj(float fov, float aspect, float nearPlane, float farPlane);

		/// Returns eye position.
		const Egg::Math::float3& GetEyePosition() override;
		/// Returns the ahead vector.
		const Egg::Math::float3& GetAhead() override;
		/// Returns the ndc-to-world-view-direction matrix to be used in shaders.
		const Egg::Math::float4x4& GetRayDirMatrix() override;
		/// Returns view matrix to be used in shaders.
		const Egg::Math::float4x4& GetViewMatrix() override;
		/// Returns projection matrix to be used in shaders.
		const Egg::Math::float4x4& GetProjMatrix() override;

		virtual void SetAspect(float aspect) override;
	GG_ENDCLASS

}}