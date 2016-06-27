#pragma once

#include <utility>
#include <memory>
#include <functional>

namespace secr { namespace dispatch {
  
    template<class Object>
    struct ownership_wrapper
    {
        using element_type = Object;
        using reference = std::add_lvalue_reference_t<element_type>;
        using const_reference = std::add_lvalue_reference_t<std::add_const_t<element_type>>;
        
        ownership_wrapper(element_type object)
        : _object(std::move(object))
        {}
        
        
        reference get() { return _object; }
        const_reference get() const { return _object; }
    private:
        element_type _object;
    };
    
    template<class Object>
    struct ownership_wrapper<Object&> : std::reference_wrapper<Object>
    {
        using std::reference_wrapper<Object>::reference_wrapper;
    };
    
    
    template<class Object, class Deleter>
    struct ownership_wrapper< std::unique_ptr<Object, Deleter> >
    {
        using element_type = Object;
        using pointer_type = std::unique_ptr<element_type, Deleter>;
        using reference = std::add_lvalue_reference_t<element_type>;
        using const_reference = std::add_lvalue_reference_t<std::add_const_t<element_type>>;
        
        ownership_wrapper(pointer_type ptr)
        : _ptr(std::move(ptr))
        {}
        
        
        reference get() { return *_ptr; }
        const_reference get() const { return *_ptr; }
    private:
        pointer_type _ptr;
    };
    
    template<class Thing>
    auto wrap_ownership(Thing&& thing)
    {
        using thing_type = std::decay_t<Thing>;
        return ownership_wrapper<thing_type>(std::forward<Thing>(thing));
    }
    
    template<class Thing>
    auto wrap_ownership(Thing& thing)
    {
        return ownership_wrapper<Thing>(thing);
    }
    
    
    
    // do not delete
    
    template<class T>
    struct null_deleter
    {
        void operator()(T*) const {}
    };
    
    template<>
    struct null_deleter<void>
    {
        template<class T>
        void operator()(T*) const {}
    };
    
    inline
    auto do_not_delete()
    {
        return null_deleter<void>();
    }
    
    template<class T>
    auto do_not_delete(T*)
    {
        return null_deleter<T>();
    }

    // ownership
    
    struct is_owner_type {
        constexpr operator bool() const { return true; }
    };
    
    static constexpr auto is_owner = is_owner_type {};
    
    struct not_owner_type {
        constexpr operator bool() const { return true; }
    };
    
    static constexpr auto not_owner = not_owner_type {};
    



}}