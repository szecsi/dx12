#include "bool3.h"
#include <cmath>

namespace Egg {
    namespace Math {

        bool3::bool3(bool x, bool y, bool z) : x { x }, y { y }, z { z }{ }

        bool3::bool3(bool x, const bool2 & yz) : x { x }, y { yz.x }, z { yz.y }{ }

        bool3::bool3(const bool2 & xy, bool z) : x { xy.x }, y { xy.y }, z { z }{ }

        bool3::bool3(const bool3 & xyz) : x { xyz.x }, y { xyz.y }, z { xyz.z }{ }

        bool3::bool3() : x{ false }, y{ false }, z{ false }{ }

        bool3 & bool3::operator=(const bool3 & rhs) noexcept {
            this->x = rhs.x;
            this->y = rhs.y;
            this->z = rhs.z;
            return *this;
        }

        bool3 & bool3::operator=(bool rhs) noexcept {
            this->x = rhs;
            this->y = rhs;
            this->z = rhs;
            return *this;
        }

        bool3 bool3::operator||(const bool3 & rhs) const noexcept {
            return bool3 { this->x || rhs.x, this->y || rhs.y, this->z || rhs.z };
        }

        bool3 bool3::operator&&(const bool3 & rhs) const noexcept {
            return bool3 { this->x && rhs.x, this->y && rhs.y, this->z && rhs.z };
        }

        bool3 bool3::operator==(const bool3 & rhs) const noexcept {
            return bool3 { this->x == rhs.x, this->y == rhs.y, this->z == rhs.z };
        }

        bool3 bool3::operator!=(const bool3 & rhs) const noexcept {
            return bool3 { this->x != rhs.x, this->y != rhs.y, this->z != rhs.z };
        }

        bool3 bool3::Random() noexcept {
            return bool3 { rand() % 2 == 0, rand() % 2 == 0, rand() % 2 == 0 };
        }

        bool bool3::Any() const noexcept {
            return x || y || z;
        }

        bool bool3::All() const noexcept {
            return x &&  y &&  z;
        }

        bool3 bool3::operator!() const noexcept {
            return bool3 {  !x,  !y,  !z };
        }

        bool3 & bool3::operator|=(const bool3 & rhs) noexcept {
            x =x || rhs.x;
            y =y || rhs.y;
            z =z || rhs.z; 
            return *this;
        }

        bool3 & bool3::operator&=(const bool3 & rhs) noexcept {
            x =x && rhs.x;
            y =y && rhs.y;
            z =z && rhs.z; 
            return *this;
        }

        const bool3 bool3::One { true, true, true };
        const bool3 bool3::Zero { false, false, false };
        const bool3 bool3::UnitX { true, false, false };
        const bool3 bool3::UnitY { false, true, false };
        const bool3 bool3::UnitZ { false, false, true };
    }
}

