#pragma once

#include <secr/dispatch/config.hpp>
#include <secr/dispatch/http/identifiers.hpp>
#include <contrib/http_parser/http_parser.h>
#include <secr/dispatch/http/request_header.hpp>
#include <secr/dispatch/fake_stream.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/log/trivial.hpp>
#include <mutex>
#include <memory>
#include <utility>

namespace secr { namespace dispatch { namespace http {

    
    struct arena_deleter
    {
        using Arena = google::protobuf::Arena;
        
        arena_deleter(Arena* arena) : _arena(arena) {}
        
        template<class T>
        void operator()(T* p) const {
            if (not _arena) {
                delete p;
            }
        }
        
        Arena* _arena;
    };
    
    template<class T> using arena_ptr = std::unique_ptr<T, arena_deleter>;
    
    template<class T, typename = void>
    struct unique_arena_ptr;
    
    struct arena_message_deleter
    {
        void operator()(google::protobuf::Message* p) const
        {
            if (p and not p->GetArena()) {
                delete p;
            }
        }
    };
    
    template<class T>
    struct unique_arena_ptr<
    T,
    std::enable_if_t<std::is_base_of<google::protobuf::Message, T>::value>
    >
    : std::unique_ptr<T, arena_message_deleter>
    {
        using std::unique_ptr<T, arena_message_deleter>::unique_ptr;
        
        static unique_arena_ptr create(google::protobuf::Arena *arena) {
            return unique_arena_ptr { google::protobuf::Arena::CreateMessage<T>(arena) };
        }
    };
    
    class request_header_manager
    {
        using Arena = ::google::protobuf::Arena;
        
        using mutex_type = std::mutex;
        using lock_type = std::unique_lock<mutex_type>;
        auto get_lock() { return lock_type(_mutex); }

        unique_arena_ptr<HttpRequestHeader> _header;
        mutable std::mutex _mutex;
        unique_arena_ptr<ContentType> _content_type = nullptr;
        
    public:
        
        const HttpRequestHeader& request_header() const {
            return *_header;
        }
        
        HttpRequestHeader& request_header() {
            return *_header;
        }
        
        request_header_manager(google::protobuf::Arena* arena)
        : _header(google::protobuf::Arena::CreateMessage<HttpRequestHeader>(arena))
        {}
        
        const ContentType& content_type() {
            auto lock = get_lock();
            if (not _content_type) {
                _content_type = _content_type.create(_header->GetArena());
                populate(*_content_type, *_header);
            }
            return *_content_type;
        }
    };
    
    struct request_context
    {
        using Arena = google::protobuf::Arena;
        
        /// @param controller_strand is the strand against which all response
        ///        back to the controller will fire.
        /// @param dispatcher_service is the service on which all requests will
        ///        be dispatched
        ///
        request_context(connection_id conn_id,
                        asio::io_service& controller_service,
                        asio::io_service& dispatcher_service);
        
        void append_uri(const char* begin, std::size_t size);
        
        void append_header_field(const char* begin,
                                 std::size_t size);
        void append_header_value(const char* begin,
                                 std::size_t size);
        
        void consume_body(const char* begin,
                                 std::size_t size);
        void notify_eof();
        
        void finalise_header(http_parser* parser);

        Arena* arena() { return std::addressof(_arena); }
        
        
        const HttpRequestHeader& request_header() { return _request_manager.request_header(); }
        HttpRequestHeader& mutable_request_header() { return _request_manager.request_header(); }
        request_header_manager& request_manager() { return _request_manager; }
        fake_stream& request_stream() { return _request_stream; }

        HttpResponseHeader& response_header() { return *_response_header; }
        fake_stream& response_stream() { return _response_stream; }
        
        asio::io_service& get_io_service();
        asio::io_service::strand& get_strand();
        
        const request_id& get_request_id() const { return _id; }
        const connection_id& get_connection_id() const { return _connection_id; }
        
        bool must_force_close_on_response() const
        {
            auto first_header = std::begin(_response_header->headers());
            auto last_header = std::end(_response_header->headers());
            auto iconnection = std::find_if(first_header,
                                            last_header,
                                            match_header_name("Connection"));
            if (iconnection == last_header)
                return true;
            
            if (boost::iequals(iconnection->value(), "close"))
                return true;
            
            if (boost::iequals(iconnection->value(), "keep-alive"))
                return false;
            auto icontent_length = std::find_if(first_header, last_header,
                                                match_header_name("Content-Length"));
            if (icontent_length != last_header)
                return false;
            
            auto iTE = std::find_if(first_header, last_header,
                                    match_header_name("Transfer-Encoding"));
            if (iTE == last_header)
                return true;
            
            if (boost::iequals(iTE->value(), "chunked"))
                return false;
            
            return true;
        }
        
    private:
        
        asio::io_service& _controller_service;
        asio::io_service& _dispatcher_service;

        Arena _arena;
        
        request_header_manager _request_manager { std::addressof(_arena) };
        arena_ptr<HttpResponseHeader> _response_header;
        
        Header * _current_header_object = nullptr;
        std::string* _current_hdr_name = nullptr;
        std::string* _current_hdr_value = nullptr;
        
        /// fake streams
        
        /// The stream which the execution context will see as the 'read' stream
        fake_stream _request_stream;

        /// The stream which the execution context will see as the 'write' stream
        fake_stream _response_stream;
        
        
        request_id _id { request_id::generate };
        connection_id _connection_id;


    };
    
}}}