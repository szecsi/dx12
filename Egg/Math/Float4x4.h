#pragma once

#include "float3.h"
#include "float4.h"
#include <cmath>
#include <cfloat>

namespace Egg {
	namespace Math {

		class float3;
		class float4;

		class float4x4
		{
		public:
			union
			{
				struct
				{
					float        _00, _01, _02, _03;
					float        _10, _11, _12, _13;
					float        _20, _21, _22, _23;
					float        _30, _31, _32, _33;
				};
				float m[4][4];
				float l[16];
			};

			float4x4() noexcept;

			float4x4(
				float _00, float _01, float _02, float _03,
				float _10, float _11, float _12, float _13,
				float _20, float _21, float _22, float _23,
				float _30, float _31, float _32, float _33) noexcept;

			static const float4x4 Identity;

			float4x4 ElementwiseProduct(const float4x4& o) const noexcept;

			float4x4 operator+(const float4x4& o) const noexcept;

			float4x4 operator-(const float4x4& o) const noexcept;

			float4x4& AssignElementwiseProduct(const float4x4& o) noexcept;

			float4x4& operator*=(float s) noexcept;

			float4x4& operator/=(float s) noexcept;

			float4x4& operator+=(const float4x4& o) noexcept;

			float4x4& operator-=(const float4x4& o) noexcept;

			float4x4 Mul(const float4x4& o) const noexcept;

			float4x4 operator<<(const float4x4& o) const noexcept;

			float4x4& operator <<=(const float4x4& o) noexcept;

			float4x4 operator*(const float4x4& o) const noexcept;

			float4x4& operator*=(const float4x4& o) noexcept;

			float4 Mul(const float4& v) const noexcept;

			float4 Transform(const float4& v) const noexcept;

			float4 operator*(const float4& v) const noexcept;

			float4x4 operator*(float s) const noexcept;

			static float4x4 Scaling(const float3& factors) noexcept;

			static float4x4 Translation(const float3& offset) noexcept;

			static float4x4 Rotation(const float3& axis, float angle) noexcept;

			static float4x4 Reflection(const float4& plane) noexcept;

			static float4x4 View(const float3& eye, const float3& ahead, const float3& up) noexcept;

			static float4x4 Proj(float fovy, float aspect, float zn, float zf) noexcept;

			// Off-center (asymmetric-frustum) projection, for stereo rendering.
			// Same row-vector z/w convention as Proj (_22=zf/(zf-zn), _32=-zn*zf/(zf-zn), _23=1);
			// left/right/bottom/top are the near-plane extents, shifted to allow asymmetric shear.
			static float4x4 ProjOffCenter(float left, float right, float bottom, float top, float zn, float zf) noexcept;

			float4x4 Transpose() const noexcept;

			float4x4 _Invert() const noexcept;

			float4x4 Invert() const noexcept;
		};

		inline float4 operator*(const float4& v, const float4x4& m) noexcept
		{
			return m.Transform(v);
		}

		inline const float4& operator*=(float4& v, const float4x4& m) noexcept
		{
			v = m.Transform(v);
			return v;
		}
	}
}


