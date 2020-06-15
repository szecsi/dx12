#pragma once

#include <boost/get_pointer.hpp>


namespace luabind { namespace detail { namespace has_get_pointer_ {
//    template<class T>
//    T * get_pointer(std::shared_ptr<T> const& p) { return p.get(); }

    template<typename T> inline T* get_pointer(const std::shared_ptr<T>& p)
    {
        return p.get();
    }

    template<typename T> inline T* get_pointer(std::shared_ptr<T>& p)
    {
        return p.get();
    }

}}}