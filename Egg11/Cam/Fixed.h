#pragma once
#include "Math/math.h"
#include "Cam/Base.h"
#include "Scene/Entity.h"

namespace Egg { namespace Cam {

	/// Camera class for entity-bound cameras.
	class Fixed :
		public Egg::Cam::Base
	{
		typedef Egg::Scene::Entity Entity;

		/// Owner entity.
		boost::weak_ptr<Entity> owner;

		/// Camera position. In entity model space if owner entity is given, in world space if it is NULL.
		Egg::Math::float3 eyePosition;
		/// Camera look at point. In entity model space if owner entity is given, in world space if it is NULL.
		Egg::Math::float3 ahead;
		/// Camera up vector. In entity model space if owner entity is given, in world space if it is NULL.
		Egg::Math::float3 up;
		/// Camera view matrix. Transforms from entity model space to camera space. (Or from world space if owner entity is NULL.)
		Egg::Math::float4x4 viewMatrix;
		/// Camera projection matrix.
		Egg::Math::float4x4 projMatrix;
		/// Camera view-direction matrix
		Egg::Math::float4x4 viewDirMatrix;

		Egg::Math::float4x4 worldViewMatrix;

		/// Field of view angle in radians.
		float fov;
		/// Aspect ratio.
		float aspect;
		/// Front clipping plane distance.
		float front;
		/// Back clipping plane distance.
		float back;

		/// Private constructor. Places camera to entity origin, uses default perspective.
		/// @param owner weak pointer to the entity the camera should move with, or NULL if camera is fixed in world space
		Fixed(boost::weak_ptr<Entity> owner);
		/// Private constructor. Uses default perspective.
		/// @param owner weak pointer to the entity the camera should move with, or NULL if camera is fixed in world space
		/// @param eyePosition camera position in owner entity's model space (or world space if owner is NULL)
		/// @param ahead camera ahead in owner entity's model space (or world space if owner is NULL)
		/// @param up camera up in owner entity's model space (or world space if owner is NULL)
		Fixed(boost::weak_ptr<Entity> owner, const Egg::Math::float3& eyePosition, const Egg::Math::float3& ahead, const Egg::Math::float3& up);
		/// Private constructor.
		/// @param owner weak pointer to the entity the camera should move with, or NULL if camera is fixed in world space
		/// @param eyePosition camera position in owner entity's model space (or world space if owner is NULL)
		/// @param ahead camera ahead in owner entity's model space (or world space if owner is NULL)
		/// @param up camera up in owner entity's model space (or world space if owner is NULL)
		/// @param fov projection field-of-view angle in radians
		/// @param aspect aspect ratio
		/// @param front front clipping plane depth
		/// @param back back clipping plane depth
		Fixed(boost::weak_ptr<Entity> owner, const Egg::Math::float3& eyePosition, const Egg::Math::float3& ahead, const Egg::Math::float3& up, double fov, double aspect, double front, double back);

	public:
		/// Local shorthand for shared pointer type.
		typedef boost::shared_ptr<Fixed> P;
		/// Local shorthand for weak pointer type.
		typedef boost::weak_ptr<Fixed> W;

		/// Factory method for instantiation. Places camera to entity origin, uses default perspective.
		/// @param owner weak pointer to the entity the camera should move with, or NULL if camera is fixed in world space
		/// @return shared pointer referencing created instance
		static P create(boost::weak_ptr<Entity> owner)
		{
			return P(new Fixed(owner));
		}

		/// Factory method for instantiation. Uses default perspective.
		/// @param owner weak pointer to the entity the camera should move with, or NULL if camera is fixed in world space
		/// @param eyePosition camera position in owner entity's model space (or world space if owner is NULL)
		/// @param ahead camera ahead in owner entity's model space (or world space if owner is NULL)
		/// @param up camera up in owner entity's model space (or world space if owner is NULL)
		/// @return shared pointer referencing created instance
		static P create(boost::weak_ptr<Entity> owner, const Egg::Math::float3& eyePosition, const Egg::Math::float3& ahead, const Egg::Math::float3& up)
		{
			return P(new Fixed(owner, eyePosition, ahead, up));
		}

		/// Factory method for instantiation.
		/// @param owner weak pointer to the entity the camera should move with, or NULL if camera is fixed in world space
		/// @param eyePosition camera position in owner entity's model space (or world space if owner is NULL)
		/// @param ahead camera ahead in owner entity's model space (or world space if owner is NULL)
		/// @param up camera up in owner entity's model space (or world space if owner is NULL)
		/// @param fov projection field-of-view angle in radians
		/// @param aspect aspect ratio
		/// @param front front clipping plane depth
		/// @param back back clipping plane depth
		/// @return shared pointer referencing created instance
		static P create(boost::weak_ptr<Entity> owner, const Egg::Math::float3& eyePosition, const Egg::Math::float3& ahead, const Egg::Math::float3& up, double fov, double aspect, double front, double back)
		{
			return P(new Fixed(owner, eyePosition, ahead, up, fov, aspect, front, back));
		}


		/// Returns camera position.
		/// @return camera position vector in world space
		const Egg::Math::float3& getEyePosition();
		/// Returns the ahead vector.
		const Egg::Math::float3& getAhead();
		/// Returns view-projection matrix.
		/// @return ndc-to-viewdir matrix
		const Egg::Math::float4x4& getViewDirMatrix();
		/// Returns view matrix.
		/// @return world-to-camera matrix
		const Egg::Math::float4x4& getViewMatrix();
		/// Returns projection matrix.
		/// @return camera-to-ndc matrix
		const Egg::Math::float4x4& getProjMatrix();

		/// Setter for fov
		void setFov(float fov);

		/// Setter for planes
		void setPlanes(float front, float back);
		/// Setter for aspect ratio.
		/// @param aspect aspect ratio
		void setAspect(float aspect);
	};

}}