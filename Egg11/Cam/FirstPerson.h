#pragma once
#include "Egg11/Math/math.h"
#include "Egg11/Cam/Base.h"

namespace Egg11 { namespace Cam {
	//TODO: shiftpressed false, gyorsul dt/t, updateproj
	GG_SUBCLASS(FirstPerson, Cam::Base)
		Egg11::Math::float3 position;
		Egg11::Math::float3 ahead;
		Egg11::Math::float3 right;
		float yaw;
		float pitch;

		float fov;
		float aspect;
		float nearPlane;
		float farPlane;

		Egg11::Math::float4x4 viewMatrix;
		Egg11::Math::float4x4 projMatrix;
		Egg11::Math::float4x4 viewDirMatrix;

		float speed;

		Egg11::Math::int2 lastMousePos;
		Egg11::Math::float2 mouseDelta;

		bool wPressed;
		bool aPressed;
		bool sPressed;
		bool dPressed;
		bool qPressed;
		bool ePressed;
		bool shiftPressed;

		void updateView();
		void updateProj();

	protected:
		FirstPerson();
	public:

		P setView(Egg11::Math::float3 position, Egg11::Math::float3 ahead);
		P setProj(float fov, float aspect, float nearPlane, float farPlane);
		P setSpeed(float speed);

		/// Returns eye position.
		const Egg11::Math::float3& getEyePosition();
		/// Returns the ahead vector.
		const Egg11::Math::float3& getAhead();
		/// Returns the ndc-to-world-view-direction matrix to be used in shaders.
		const Egg11::Math::float4x4& getViewDirMatrix();
		/// Returns view matrix to be used in shaders.
		const Egg11::Math::float4x4& getViewMatrix();
		/// Returns projection matrix to be used in shaders.
		const Egg11::Math::float4x4& getProjMatrix();

		Egg11::Math::float4x4 getOrthoProjMatrix(float worldWidth, float wolrdHeight);

		/// Moves camera. To be implemented if the camera has its own animation mechanism.
		virtual void animate(double dt);

		virtual void processMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

		virtual void setAspect(float aspect);
	GG_ENDCLASS

}}