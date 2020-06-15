#pragma once
#include "../Common.h"
#include "Egg/Math/math.h"
#include "Base.h"
#include "Egg/Scene/Entity.h"

namespace Egg { namespace Cam {

	GG_SUBCLASS( Fixed, Egg::Cam::Base)
		typedef Egg::Scene::Entity Entity;

		/// Owner entity.
		Entity::W owner;

		/// Camera position. In entity model space if owner entity is given, in world space if it is NULL.
		Egg::Math::Float3 eyePosition;
		/// Camera look at point. In entity model space if owner entity is given, in world space if it is NULL.
		Egg::Math::Float3 ahead;
		/// Camera up vector. In entity model space if owner entity is given, in world space if it is NULL.
		Egg::Math::Float3 up;
		/// Camera view matrix. Transforms from entity model space to camera space. (Or from world space if owner entity is NULL.)
		Egg::Math::Float4x4 viewMatrix;
		/// Camera projection matrix.
		Egg::Math::Float4x4 projMatrix;

		Egg::Math::Float4x4 viewMatrixWorld;
		Egg::Math::Float4x4 rayDirMatrix;

		/// Field of view angle in radians.
		float fov;
		/// Aspect ratio.
		float aspect;
		/// Front clipping plane distance.
		float front;
		/// Back clipping plane distance.
		float back;

protected:
		/// Private constructor. Places camera to entity origin, uses default perspective.
		/// @param owner weak pointer to the entity the camera should move with, or NULL if camera is fixed in world space
		Fixed(Entity::W owner);
		/// Private constructor. Uses default perspective.
		/// @param owner weak pointer to the entity the camera should move with, or NULL if camera is fixed in world space
		/// @param eyePosition camera position in owner entity's model space (or world space if owner is NULL)
		/// @param ahead camera ahead in owner entity's model space (or world space if owner is NULL)
		/// @param up camera up in owner entity's model space (or world space if owner is NULL)
		Fixed(Entity::W owner, const Egg::Math::Float3& eyePosition, const Egg::Math::Float3& ahead, const Egg::Math::Float3& up);
		/// Private constructor.
		/// @param owner weak pointer to the entity the camera should move with, or NULL if camera is fixed in world space
		/// @param eyePosition camera position in owner entity's model space (or world space if owner is NULL)
		/// @param ahead camera ahead in owner entity's model space (or world space if owner is NULL)
		/// @param up camera up in owner entity's model space (or world space if owner is NULL)
		/// @param fov projection field-of-view angle in radians
		/// @param aspect aspect ratio
		/// @param front front clipping plane depth
		/// @param back back clipping plane depth
		Fixed(Entity::W owner, const Egg::Math::Float3& eyePosition, const Egg::Math::Float3& ahead, const Egg::Math::Float3& up, double fov, double aspect, double front, double back);

	public:

		/// Returns camera position.
		/// @return camera position vector in world space
		const Egg::Math::Float3& GetEyePosition();
		/// Returns the ahead vector.
		const Egg::Math::Float3& GetAhead();
		/// Returns view-projection matrix.
		/// @return ndc-to-viewdir matrix
		const Egg::Math::Float4x4& GetRayDirMatrix();
		/// Returns view matrix.
		/// @return world-to-camera matrix
		const Egg::Math::Float4x4& GetViewMatrix();
		/// Returns projection matrix.
		/// @return camera-to-ndc matrix
		const Egg::Math::Float4x4& GetProjMatrix();


		/// Setter for aspect ratio.
		/// @param aspect aspect ratio
		void SetAspect(float aspect);
	GG_ENDCLASS

}}