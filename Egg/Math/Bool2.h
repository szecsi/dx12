#pragma once

#include "bool2Swizzle.hpp"
#include "bool3Swizzle.hpp"
#include "bool4Swizzle.hpp"
#include "bool3.h"
#include "bool4.h"

namespace Egg {
    namespace Math {

        class bool3;
        class bool4;

        class bool2 {
        public:
            union {
                struct {
                    bool x;
                    bool y;
                };

                bool2Swizzle<bool2, 2, 0, 0> xx;
                bool2Swizzle<bool2, 2, 0, 1> xy;
                bool2Swizzle<bool2, 2, 1, 0> yx;
                bool2Swizzle<bool2, 2, 1, 1> yy;

                bool3Swizzle<bool3, 2, 0, 0, 0> xxx;
                bool3Swizzle<bool3, 2, 0, 0, 1> xxy;
                bool3Swizzle<bool3, 2, 0, 1, 0> xyx;
                bool3Swizzle<bool3, 2, 0, 1, 1> xyy;
                bool3Swizzle<bool3, 2, 1, 0, 0> yxx;
                bool3Swizzle<bool3, 2, 1, 0, 1> yxy;
                bool3Swizzle<bool3, 2, 1, 1, 0> yyx;
                bool3Swizzle<bool3, 2, 1, 1, 1> yyy;

                bool4Swizzle<bool4, 2, 0, 0, 0, 0> xxxx;
                bool4Swizzle<bool4, 2, 0, 0, 1, 0> xxxy;
                bool4Swizzle<bool4, 2, 0, 0, 0, 1> xxyx;
                bool4Swizzle<bool4, 2, 0, 0, 1, 1> xxyy;
                bool4Swizzle<bool4, 2, 0, 1, 0, 0> xyxx;
                bool4Swizzle<bool4, 2, 0, 1, 1, 0> xyxy;
                bool4Swizzle<bool4, 2, 0, 1, 0, 1> xyyx;
                bool4Swizzle<bool4, 2, 0, 1, 1, 1> xyyy;
                bool4Swizzle<bool4, 2, 1, 0, 0, 0> yxxx;
                bool4Swizzle<bool4, 2, 1, 0, 1, 0> yxxy;
                bool4Swizzle<bool4, 2, 1, 0, 0, 1> yxyx;
                bool4Swizzle<bool4, 2, 1, 0, 1, 1> yxyy;
                bool4Swizzle<bool4, 2, 1, 1, 0, 0> yyxx;
                bool4Swizzle<bool4, 2, 1, 1, 1, 0> yyxy;
                bool4Swizzle<bool4, 2, 1, 1, 0, 1> yyyx;
                bool4Swizzle<bool4, 2, 1, 1, 1, 1> yyyy;
            };

            bool2(bool x, bool y);

            bool2(const bool2 & xy);

            bool2();

            bool2 & operator=(const bool2 & rhs) noexcept;
bool2 & operator=(bool rhs) noexcept;

            bool2 operator||(const bool2 & rhs) const noexcept;

            bool2 operator&&(const bool2 & rhs) const noexcept;

            bool2 operator==(const bool2 & rhs) const noexcept;

            bool2 operator!=(const bool2 & rhs) const noexcept;

            static bool2 Random() noexcept;

            bool Any() const noexcept;

            bool All() const noexcept;

            bool2 operator!() const noexcept;

            bool2 & operator|=(const bool2 & rhs) noexcept;

            bool2 & operator&=(const bool2 & rhs) noexcept;

            static const bool2 One;
            static const bool2 Zero;
            static const bool2 UnitX;
            static const bool2 UnitY;
        };
    }
}

