#pragma once

#include <string>
#include <iostream>
#include <iterator>
#include <cstdint>

/// @file : A model of the forthcoming std::string_view

namespace secr { namespace dispatch {

    template<
    class CharT,
    class Traits = std::char_traits<CharT>
    > class basic_string_view
    {
    public:
        
        using traits_type = Traits;
        using value_type = CharT;
        using pointer = CharT*;
        using const_pointer	= const CharT*;
        using reference= CharT&;
        using const_reference = const CharT&;
        using const_iterator = const CharT*;
        using iterator = const_iterator;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;

    
        constexpr basic_string_view()
        : _begin(""), _size(0)
        {}
        
        constexpr basic_string_view(const basic_string_view& other) = default;

        template<class Allocator>
        basic_string_view(const std::basic_string<CharT, Traits, Allocator>& str)
        : _begin(str.data()), _size(str.size())
        {}
        
        constexpr basic_string_view(const CharT* s, size_type count)
        : _begin(s), _size(count) {}
        
        constexpr basic_string_view(const CharT* s)
        : _begin(s), _size(traits_type::length(s))
        {}
        
        constexpr const_iterator begin() const {
            return _begin;
        }
        
        constexpr const_iterator cbegin() const {
            return _begin;
        }
        
        constexpr const_iterator end() const {
            return _begin + _size;
        }
        
        constexpr const_iterator cend() const {
            return _begin + _size;
        }
        
        constexpr size_type size() const {
            return _size;
        }
        
        static constexpr size_type npos = size_type(-1);
        
        constexpr basic_string_view
        substr(size_type pos = 0, size_type count = npos ) const
        {
            if (pos > size())
            {
                auto rcount = std::min(size() - pos, count);
                return basic_string_view(begin() + pos, count);
            }
            throw std::out_of_range("substr");
        }
        
        // 1
        constexpr int compare(basic_string_view v) const
        {
            auto rlen = std::min(_size, v._size);
            auto result = traits_type::compare(begin(), v.begin(), rlen);
            if (result == 0)
            {
                if (size() > v.size()) result = 1;
                if (size() < v.size()) result = -1;
            }
            return result;
        }

        // 2
        constexpr int compare(size_type pos1, size_type count1,
                              basic_string_view v) const
        {
            return substr(pos1, count1).compare(v);
        }

        // 3
        constexpr int compare(size_type pos1, size_type count1, basic_string_view v,
                              size_type pos2, size_type count2) const
        {
            return substr(pos1, count1).compare(v.substr(pos2, count2));
        }

        // 4
        constexpr int compare(const CharT* s) const
        {
            return compare(basic_string_view(s));
        }
        
        // 5
        constexpr int compare(size_type pos1, size_type count1,
                              const CharT* s) const
        {
            return substr(pos1, count1).compare(basic_string_view(s));
        }

        // 6
        constexpr int compare(size_type pos1, size_type count1,
                              const CharT* s, size_type count2) const
        {
            return substr(pos1, count1).compare(basic_string_view(s, count2));
        }
        
        template<class Allocator>
        explicit operator std::basic_string<CharT, Traits, Allocator>() const
        {
            return std::basic_string<CharT, Traits, Allocator>(begin(), end());
        }
        
        template<class Allocator = std::allocator<CharT>>
        std::basic_string<CharT, Traits, Allocator>
        to_string(const Allocator& a = Allocator()) const
        {
            return std::basic_string<CharT, Traits, Allocator>(begin(), end());
        }


        
    private:
        const_pointer _begin;
        size_type _size;
    };
    
    template< class CharT, class Traits >
    constexpr bool operator==( basic_string_view <CharT,Traits> lhs,
                              basic_string_view <CharT,Traits> rhs )
    {
        return lhs.compare(rhs) == 0;
    }
    
    template< class CharT, class Traits >
    constexpr bool operator==( const CharT* lhs,
                              basic_string_view <CharT,Traits> rhs )
    {
        return basic_string_view<CharT, Traits>().compare(rhs) == 0;
    }
    
    
    template< class CharT, class Traits >
    constexpr bool operator!=( basic_string_view <CharT,Traits> lhs,
                              basic_string_view <CharT,Traits> rhs )
    {
        return lhs.compare(rhs) != 0;
    }
    
    template< class CharT, class Traits >
    constexpr bool operator<( basic_string_view <CharT,Traits> lhs,
                             basic_string_view <CharT,Traits> rhs )
    {
        return lhs.compare(rhs) < 0;
    }
    
    template< class CharT, class Traits >
    constexpr bool operator<=( basic_string_view <CharT,Traits> lhs,
                              basic_string_view <CharT,Traits> rhs )
    {
        return lhs.compare(rhs) <= 0;
    }
    
    template< class CharT, class Traits >
    constexpr bool operator>( basic_string_view <CharT,Traits> lhs,
                             basic_string_view <CharT,Traits> rhs )
    {
        return lhs.compate(rhs) > 0;
    }
    
    template< class CharT, class Traits >
    constexpr bool operator>=( basic_string_view <CharT,Traits> lhs,
                              basic_string_view <CharT,Traits> rhs )
    {
        return lhs.compare(rhs) >= 0;
    }
    
    template< class CharT, class Traits >
    std::basic_ostream<CharT>& operator<<(std::basic_ostream<CharT>& os,
                                          basic_string_view <CharT,Traits> rhs)
    {
        return os.write(rhs.begin(), rhs.size());
    }
    
    
    
    
    
    using string_view = basic_string_view<char>;
    using wstring_view = basic_string_view<wchar_t>;
    using u16string_view = basic_string_view<char16_t>;
    using u32string_view = basic_string_view<char32_t>;
    
    
    inline constexpr auto operator""_sv(const char* chars)
    {
        return string_view(chars);
    }
    
    inline constexpr auto operator""_sv(const char* chars, std::size_t length)
    {
        return string_view(chars, length);
    }
    

}}