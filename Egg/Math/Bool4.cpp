#include "bool4.h"
#include <cmath>

namespace Egg {
    namespace Math {

        bool4::bool4(bool x, bool y, bool z, bool w) : x { x }, y { y }, z { z }, w { w }{ }

        bool4::bool4(bool x, bool y, const bool2 & zw) : x { x }, y { y }, z { zw.x }, w { zw.y }{ }

        bool4::bool4(const bool2 & xy, const bool2 & zw) : x { xy.x }, y { xy.y }, z { zw.x }, w { zw.y }{ }

        bool4::bool4(const bool2 & xy, bool z, bool w) : x { xy.x }, y { xy.y }, z { z }, w { w }{ }

        bool4::bool4(const bool3 & xyz, bool w) : x { xyz.x }, y { xyz.y }, z { xyz.z }, w { w }{ }

        bool4::bool4(bool x, const bool3 & yzw) : x { x }, y { yzw.x }, z { yzw.y }, w { yzw.z }{ }

        bool4::bool4(const bool4 & xyzw) : x { xyzw.x }, y { xyzw.y }, z { xyzw.z }, w { xyzw.w }{ }

        bool4::bool4() : x{ false }, y{ false }, z{ false }, w{ false }{ }

        bool4 & bool4::operator=(const bool4 & rhs) noexcept {
            this->x = rhs.x;
            this->y = rhs.y;
            this->z = rhs.z;
            this->w = rhs.w;
            return *this;
        }

        bool4 & bool4::operator=(bool rhs) noexcept {
            this->x = rhs;
            this->y = rhs;
            this->z = rhs;
            this->w = rhs;
            return *this;
        }

        bool4 bool4::operator||(const bool4 & rhs) const noexcept {
            return bool4 { this->x || rhs.x, this->y || rhs.y, this->z || rhs.z, this->w || rhs.w };
        }

        bool4 bool4::operator&&(const bool4 & rhs) const noexcept {
            return bool4 { this->x && rhs.x, this->y && rhs.y, this->z && rhs.z, this->w && rhs.w };
        }

        bool4 bool4::operator==(const bool4 & rhs) const noexcept {
            return bool4 { this->x == rhs.x, this->y == rhs.y, this->z == rhs.z, this->w == rhs.w };
        }

        bool4 bool4::operator!=(const bool4 & rhs) const noexcept {
            return bool4 { this->x != rhs.x, this->y != rhs.y, this->z != rhs.z, this->w != rhs.w };
        }

        bool4 bool4::Random() noexcept {
            return bool4 { rand() % 2 == 0, rand() % 2 == 0, rand() % 2 == 0, rand() % 2 == 0 };
        }

        bool bool4::Any() const noexcept {
            return x || y || z || w;
        }

        bool bool4::All() const noexcept {
            return x &&  y &&  z &&  w;
        }

        bool4 bool4::operator!() const noexcept {
            return bool4 {  !x,  !y,  !z,  !w };
        }

        bool4 & bool4::operator|=(const bool4 & rhs) noexcept {
            x =x || rhs.x;
            y =y || rhs.y;
            z =z || rhs.z;
            w =w || rhs.w; 
            return *this;
        }

        bool4 & bool4::operator&=(const bool4 & rhs) noexcept {
            x =x && rhs.x;
            y =y && rhs.y;
            z =z && rhs.z;
            w =w && rhs.w; 
            return *this;
        }

        const bool4 bool4::Zero { false, false, false, false };
        const bool4 bool4::UnitX { true, false, false, false };
        const bool4 bool4::UnitY { false, true, false, false };
        const bool4 bool4::UnitZ { false, false, true, false };
        const bool4 bool4::UnitW { false, false, false, true };
        const bool4 bool4::One { true, true, true, true };
    }
}

