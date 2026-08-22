#pragma once
#include "../Math/Math.h"
#include "../Common.h"
#include "Base.h"

namespace Egg { namespace Cam {
	//TODO: shiftpressed false, gyorsul dt/t
	GG_SUBCLASS(FirstPerson, Cam::Base)
		Egg::Math::float3 position;
		Egg::Math::float3 ahead;
		Egg::Math::float3 right;
		float yaw;
		float pitch;

		float fov;
		float aspect;
		float nearPlane;
		float farPlane;

		Egg::Math::float4x4 viewMatrix;
		Egg::Math::float4x4 projMatrix;
		Egg::Math::float4x4 rayDirMatrix;

		float speed;

		Egg::Math::int2 lastMousePos;
		Egg::Math::float2 mouseDelta;

		bool wPressed;
		bool aPressed;
		bool sPressed;
		bool dPressed;
		bool qPressed;
		bool ePressed;
		bool shiftPressed;

		void UpdateView();
		void UpdateProj();

	protected:
		FirstPerson();
	public:

		P SetView(Egg::Math::float3 position, Egg::Math::float3 ahead);
		P SetProj(float fov, float aspect, float nearPlane, float farPlane);
		P SetSpeed(float speed);

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

		/// Moves camera. To be implemented if the camera has its own animation mechanism.
		virtual void Animate (double dt) override;

		virtual void ProcessMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;

		virtual void SetAspect(float aspect) override;
	GG_ENDCLASS

}}