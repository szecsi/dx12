#pragma once

#include "bool2Swizzle.hpp"
#include "bool3Swizzle.hpp"
#include "bool4Swizzle.hpp"

namespace Egg {
    namespace Math {

        class bool1 {
        public:
            union {
                struct {
                    bool x;
                };
            };

            bool1(bool x);

            bool1();

            bool1 & operator=(const bool1 & rhs) noexcept;
bool1 & operator=(bool rhs) noexcept;

            bool1 operator||(const bool1 & rhs) const noexcept;

            bool1 operator&&(const bool1 & rhs) const noexcept;

            bool1 operator==(const bool1 & rhs) const noexcept;

            bool1 operator!=(const bool1 & rhs) const noexcept;

            static bool1 Random() noexcept;

            bool Any() const noexcept;

            bool All() const noexcept;

            bool1 operator!() const noexcept;

            bool1 & operator|=(const bool1 & rhs) noexcept;

            bool1 & operator&=(const bool1 & rhs) noexcept;

        };
    }
}

