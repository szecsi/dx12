#include "bool1.h"
#include <cmath>

namespace Egg {
    namespace Math {

        bool1::bool1(bool x) : x { x }{ }

        bool1::bool1() : x{ false }{ }

        bool1 & bool1::operator=(const bool1 & rhs) noexcept {
            this->x = rhs.x;
            return *this;
        }

        bool1 & bool1::operator=(bool rhs) noexcept {
            this->x = rhs;
            return *this;
        }

        bool1 bool1::operator||(const bool1 & rhs) const noexcept {
            return bool1 { this->x || rhs.x };
        }

        bool1 bool1::operator&&(const bool1 & rhs) const noexcept {
            return bool1 { this->x && rhs.x };
        }

        bool1 bool1::operator==(const bool1 & rhs) const noexcept {
            return bool1 { this->x == rhs.x };
        }

        bool1 bool1::operator!=(const bool1 & rhs) const noexcept {
            return bool1 { this->x != rhs.x };
        }

        bool1 bool1::Random() noexcept {
            return bool1 { rand() % 2 == 0 };
        }

        bool bool1::Any() const noexcept {
            return x;
        }

        bool bool1::All() const noexcept {
            return x;
        }

        bool1 bool1::operator!() const noexcept {
            return bool1 {  !x };
        }

        bool1 & bool1::operator|=(const bool1 & rhs) noexcept {
            x =x || rhs.x; 
            return *this;
        }

        bool1 & bool1::operator&=(const bool1 & rhs) noexcept {
            x =x && rhs.x; 
            return *this;
        }

    }
}

