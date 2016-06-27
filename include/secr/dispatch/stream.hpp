#pragma once

#include <secr/dispatch/config.hpp>
#include <secr/dispatch/ownership.hpp>
#include <secr/dispatch/polymorphic_stream.hpp>
#include <memory>

namespace secr { namespace dispatch {
    
    template<class T, class Deleter = std::default_delete<T> >
    struct delete_if_owner
    {
        delete_if_owner(bool owner, Deleter deleter = Deleter())
        : _deleter(std::move(deleter))
        , _owner(owner)
        {}
        
        void operator()(T* p) const {
            if (_owner)
                _deleter(p);
        }
        
        bool owner() const { return _owner; }
        
        const Deleter& get_deleter() const {
            return _deleter;
        }
        
        Deleter& get_deleter() {
            return _deleter;
        }
        
    private:
        Deleter _deleter;
        bool _owner;
    };
    
    template<class Type, class Deleter> using delete_if_owner_ptr = std::unique_ptr<Type, delete_if_owner<Type, Deleter>>;
    
    template<
    class Type,
    class Deleter,
    std::enable_if_t< not std::is_base_of<delete_if_owner<Type>, Deleter>::value > * = nullptr
    >
    auto make_delete_if_owner_ptr(std::unique_ptr<Type, Deleter> p)
    {
        return delete_if_owner_ptr<Type, Deleter> {
            p.release(),
            delete_if_owner<Type, Deleter>(true,
                                           std::move(p.get_deleter()))
        };
    }
    
    
    
    
    /// @tparam StreamType is a stream object that implements the
    /// asio::AsyncReadStream concept
    
    
    
    
    
}}