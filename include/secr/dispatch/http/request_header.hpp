#pragma once

#include <secr/dispatch/config.hpp>
#include <secr/dispatch/buffered_stream.hpp>
#include <secr/dispatch/secr_dispatch_http.pb.h>
#include <contrib/http_parser/http_parser.h>
#include <functional>
#include <tuple>

namespace secr { namespace dispatch { namespace http {
    
    
    template<class StringLike>
    struct unary_match_header_name
    {
        using pred = std::equal_to<>;
        unary_match_header_name(StringLike value) : r(std::move(value)) {}
        
        bool operator()(const Header& l) const {
            return _pred(l.name(), r);
        }
        pred _pred;
        const StringLike r;
    };
    
    template<class StringLike>
    auto match_header_name(StringLike&& r)
    {
        using string_type = std::decay_t<StringLike>;
        using unary_function_type = unary_match_header_name<string_type>;
        return unary_function_type(std::forward<StringLike>(r));
    }
    
    struct binary_match_header_name
    {
        using pred = std::equal_to<>;
        bool operator()(const Header& l, const Header& r) const {
            return _pred(l.name(), r.name());
        }
        
        bool operator()(const Header& l, const std::string& r) const {
            return _pred(l.name(), r);
        }
        
        bool operator()(const std::string& l, const Header& r) const {
            return _pred(l, r.name());
        }
        pred _pred;
    };
 
    inline
    auto match_header_name() {
        return binary_match_header_name();
    }
    
    struct protocol_error
    : std::runtime_error
    {
        protocol_error(http_errno en)
        : std::runtime_error::runtime_error(http_errno_name(en))
        {}
        
        using std::runtime_error::runtime_error;
        
    };
    
    const Header* find_only_header_like(const google::protobuf::RepeatedPtrField<Header>& headers, const std::string& like);
    const Header& require_only_header_like(const google::protobuf::RepeatedPtrField<Header>& headers, const std::string& like);
    std::vector<std::reference_wrapper<const Header>>
    find_headers_like(const google::protobuf::RepeatedPtrField<Header>& headers,
                      const std::string& like);
    
    /// Set a header's value only if that header does not already exist
    /// The valus is not escaped. the caller must guarantee that it's correct
    void set_unique_header(HttpRequestHeader& request, const std::string& name, const std::string& value);

    // HttpRequest
    
    const Header* find_only_header_like(const HttpRequestHeader& request,
                                        const std::string& like);
    const Header& require_only_header_like(const HttpRequestHeader& request,
                                           const std::string& like);
    std::vector<std::reference_wrapper<const Header>>
    find_headers_like(const HttpRequestHeader& request,
                      const std::string& like);
    
    ContentType& populate(ContentType& ct, const std::string& header_value);
    ContentType& populate(ContentType& ct, const HttpRequestHeader& header_value);


    // HttpResponse
    
    struct shared_byte_buffer
    {
        using element_type = std::uint8_t;
        using ptr_type = std::shared_ptr<element_type>;
        
        shared_byte_buffer(ptr_type ptr, std::size_t size)
        : _ptr(std::move(ptr)), _size(size)
        {}
        
        element_type* data() { return _ptr.get(); }
        const element_type* data() const { return _ptr.get(); }
        std::size_t size() const { return _size; }
        
        element_type* begin() { return _ptr.get(); }
        const element_type* begin() const { return _ptr.get(); }
        element_type* end() { return begin() + size(); }
        const element_type* end() const { return begin() + size(); }

    private:
        ptr_type _ptr;
        std::size_t _size;
    };
    
    struct arena_byte_buffer
    {
        using element_type = std::uint8_t;
        struct deleter {
            deleter(google::protobuf::Arena* arena) : arena(arena) {}
            
            void operator()(element_type*p) const {
                if (not arena)
                    delete[] p;
            }
            google::protobuf::Arena* arena;
        };
        using ptr_type = std::unique_ptr<element_type[], deleter>;
        
        arena_byte_buffer(google::protobuf::Arena* arena, std::size_t size)
        : _ptr(google::protobuf::Arena::CreateArray<element_type>(arena, size), deleter(arena))
        , _size(size)
        {
            
        }
        
        element_type* data() { return _ptr.get(); }
        const element_type* data() const { return _ptr.get(); }
        std::size_t size() const { return _size; }

        element_type* begin() { return _ptr.get(); }
        const element_type* begin() const { return _ptr.get(); }
        element_type* end() { return begin() + size(); }
        const element_type* end() const { return begin() + size(); }
        
        shared_byte_buffer shared() &&
        {
            return {
                std::shared_ptr<element_type>(_ptr.release(),
                                              _ptr.get_deleter()),
                _size };
        };


    private:
        ptr_type _ptr;
        std::size_t _size;
    };
    
    
    arena_byte_buffer to_response_buffer(const HttpResponseHeader& rmsg);
    arena_byte_buffer to_response_buffer(const HttpResponseHeader* rmsg);
    
    void set_status(HttpResponseHeader* rmsg, int code, const std::string& message);
    void set_status(HttpResponseHeader& rmsg, int code, const std::string& message);

    void add_header(HttpResponseHeader* rmsg, const std::string& name, const std::string& value);
    void add_header(HttpResponseHeader& rmsg, const std::string& name, const std::string& message);

    /// replace all ocurrences of a given header name with just one header of
    /// the same name with a new value.
    Header& set_header(HttpResponseHeader& response,
                       const std::string& name,
                       const std::string& value);

    inline
    auto http_version(const HttpResponseHeader& h)
    {
        return std::make_tuple(h.version_major(), h.version_minor());
    }
    
    inline
    auto http_version(const HttpRequestHeader& h)
    {
        return std::make_tuple(h.version_major(), h.version_minor());
    }
    
}}}

namespace boost { namespace asio {
  
    inline
    const_buffers_1 buffer(const secr::dispatch::http::arena_byte_buffer& abb)
    {
        return { abb.data(), abb.size() };
    }
    
    inline
    mutable_buffers_1 buffer(secr::dispatch::http::arena_byte_buffer& abb)
    {
        return { abb.data(), abb.size() };
    }

    inline
    const_buffers_1 buffer(const secr::dispatch::http::shared_byte_buffer& sbb)
    {
        return { sbb.data(), sbb.size() };
    }
    
    inline
    mutable_buffers_1 buffer(secr::dispatch::http::shared_byte_buffer& sbb)
    {
        return { sbb.data(), sbb.size() };
    }
}}