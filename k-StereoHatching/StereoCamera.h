#pragma once

#include <Egg/Cam/FirstPerson.h>
#include <Egg/Math/Float3.h>
#include <Egg/Math/Float4x4.h>
#include <cmath>

// Derives off-axis (asymmetric-frustum) stereo eye view/projection matrices
// from ONE shared FirstPerson "head" camera, rather than driving two
// independent cameras. Two independently-driven cameras would need every
// WASD/mouse update applied identically to both and could drift; deriving
// both eyes statelessly from one authoritative head pose each frame
// guarantees identical orientation with no synchronization code at all.
namespace Hatch {

	inline Egg::Math::float3 HeadRight(const Egg::Cam::FirstPerson::P& head) {
		using namespace Egg::Math;
		// Matches float4x4::View's own internal xaxis construction exactly
		// (xaxis = up.Cross(zaxis)), so this "right" is consistent with
		// whatever View() will do with the same ahead vector.
		return float3::UnitY.Cross(head->GetAhead()).Normalize();
	}

	inline Egg::Math::float4x4 ComputeEyeView(const Egg::Cam::FirstPerson::P& head, float eyeSeparation, bool isRightEye) {
		using namespace Egg::Math;
		float3 eye = head->GetEyePosition();
		float3 ahead = head->GetAhead();
		float3 right = HeadRight(head);
		float3 eyePos = eye + right * (isRightEye ? 0.5f : -0.5f) * eyeSeparation;
		// SAME ahead for both eyes -- deliberately not toe-in.
		return float4x4::View(eyePos, ahead, float3::UnitY);
	}

	inline Egg::Math::float4x4 ComputeEyeProj(float fov, float aspect, float zn, float zf,
		float eyeSeparation, float convergenceDistance, bool isRightEye) {
		using namespace Egg::Math;
		float hNear = zn * std::tan(fov * 0.5f);
		float wNear = hNear * aspect;
		float frustumShift = 0.5f * eyeSeparation * zn / convergenceDistance;
		float shift = isRightEye ? -frustumShift : frustumShift;
		float l = -wNear + shift;
		float r = wNear + shift;
		return float4x4::ProjOffCenter(l, r, -hNear, hNear, zn, zf);
	}

}
