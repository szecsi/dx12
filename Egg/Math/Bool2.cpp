#include "bool2.h"
#include <cmath>

namespace Egg {
    namespace Math {

        bool2::bool2(bool x, bool y) : x { x }, y { y }{ }

        bool2::bool2(const bool2 & xy) : x { xy.x }, y { xy.y }{ }

        bool2::bool2() : x{ false }, y{ false }{ }

        bool2 & bool2::operator=(const bool2 & rhs) noexcept {
            this->x = rhs.x;
            this->y = rhs.y;
            return *this;
        }

        bool2 & bool2::operator=(bool rhs) noexcept {
            this->x = rhs;
            this->y = rhs;
            return *this;
        }

        bool2 bool2::operator||(const bool2 & rhs) const noexcept {
            return bool2 { this->x || rhs.x, this->y || rhs.y };
        }

        bool2 bool2::operator&&(const bool2 & rhs) const noexcept {
            return bool2 { this->x && rhs.x, this->y && rhs.y };
        }

        bool2 bool2::operator==(const bool2 & rhs) const noexcept {
            return bool2 { this->x == rhs.x, this->y == rhs.y };
        }

        bool2 bool2::operator!=(const bool2 & rhs) const noexcept {
            return bool2 { this->x != rhs.x, this->y != rhs.y };
        }

        bool2 bool2::Random() noexcept {
            return bool2 { rand() % 2 == 0, rand() % 2 == 0 };
        }

        bool bool2::Any() const noexcept {
            return x || y;
        }

        bool bool2::All() const noexcept {
            return x &&  y;
        }

        bool2 bool2::operator!() const noexcept {
            return bool2 {  !x,  !y };
        }

        bool2 & bool2::operator|=(const bool2 & rhs) noexcept {
            x =x || rhs.x;
            y =y || rhs.y; 
            return *this;
        }

        bool2 & bool2::operator&=(const bool2 & rhs) noexcept {
            x =x && rhs.x;
            y =y && rhs.y; 
            return *this;
        }

        const bool2 bool2::One { true, true };
        const bool2 bool2::Zero { false, false };
        const bool2 bool2::UnitX { true, false };
        const bool2 bool2::UnitY { false, true };
    }
}

