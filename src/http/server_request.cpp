#include <secr/dispatch/http/server_connection.hpp>
#include <secr/dispatch/http/server_request.hpp>


namespace secr { namespace dispatch { namespace http {
    
    struct http_stream_concept
    {
        using read_handler = std::function<void(const error_code& , std::size_t)>;
        using write_handler = std::function<void(const error_code& , std::size_t)>;
        
        /// read as many bytes as are available
        virtual void async_read_some(asio::mutable_buffers_1 buffer, read_handler handler) = 0;
        
        virtual void async_write_some(asio::const_buffers_1 buffer, write_handler) = 0;
    };
    

    request_context::request_context(connection_id conn_id,
                                     asio::io_service& controller_service,
                                     asio::io_service& dispatcher_service)
    : _controller_service(controller_service)
    , _dispatcher_service(dispatcher_service)
    , _request_manager(arena())
    , _response_header{ Arena::CreateMessage<HttpResponseHeader>(arena()), arena() }
    , _request_stream(_dispatcher_service, _controller_service)
    , _response_stream(controller_service, _dispatcher_service)
    , _connection_id(std::move(conn_id))
    {}
    
    
    
    void request_context::append_uri(const char* begin, std::size_t size)
    {
        _request_manager.request_header().mutable_uri()->append(begin, size);
    }

    void request_context::append_header_field(const char* begin,
                             std::size_t size)
    {
        if (_current_hdr_value)
        {
            _current_header_object = nullptr;
            _current_hdr_value = nullptr;
        }
        if (not _current_hdr_name) {
            _current_header_object = mutable_request_header().add_headers();
            _current_hdr_name = _current_header_object->mutable_name();
        }
        _current_hdr_name->append(begin, size);
    }
    
    void request_context::append_header_value(const char* begin,
                             std::size_t size)
    {
        if (_current_hdr_name) {
            _current_hdr_name = nullptr;
        }
        assert(_current_header_object);
        if (not _current_hdr_value) {
            _current_hdr_value = _current_header_object->mutable_value();
        }
        _current_hdr_value->append(begin, size);
    }
    
    namespace
    {
        void check_url_field(http_parser_url& url_parser,
                             HttpRequestHeader& msg,
                             std::string* (HttpRequestHeader::QueryParts::*locate)(),
                             int field)
        {
            if (url_parser.field_set & (1 << field))
            {
                auto& slot = url_parser.field_data[field];
                std::size_t offset = slot.off;
                std::size_t length = slot.len;
                auto source = msg.uri().data();
                auto& parts = *(msg.mutable_query());
                (parts.*locate)()->append(source + offset, length);
            }
        }
    }

    void request_context::finalise_header(http_parser* parser)
    {
        _current_hdr_value = nullptr;
        _current_hdr_name = nullptr;
        _current_header_object = nullptr;
        
        http_parser_url url_parser;
        http_parser_url_init(std::addressof(url_parser));
        auto result = http_parser_parse_url(mutable_request_header().uri().data(),
                                            mutable_request_header().uri().size(),
                                            parser->method == HTTP_CONNECT,
                                            std::addressof(url_parser));
        if (result) {
            BOOST_LOG_TRIVIAL(info) << "request_context::finalise_header - invalid url\n" << api::as_json(request_header());
            throw invalid_url(request_header().uri());
        }
        
        check_url_field(url_parser, mutable_request_header(), &HttpRequestHeader::QueryParts::mutable_schema, UF_SCHEMA);
        check_url_field(url_parser, mutable_request_header(), &HttpRequestHeader::QueryParts::mutable_host, UF_HOST);
        check_url_field(url_parser, mutable_request_header(), &HttpRequestHeader::QueryParts::mutable_port, UF_PORT);
        check_url_field(url_parser, mutable_request_header(), &HttpRequestHeader::QueryParts::mutable_path, UF_PATH);
        check_url_field(url_parser, mutable_request_header(), &HttpRequestHeader::QueryParts::mutable_query, UF_QUERY);
        check_url_field(url_parser, mutable_request_header(), &HttpRequestHeader::QueryParts::mutable_fragment, UF_FRAGMENT);
        check_url_field(url_parser, mutable_request_header(), &HttpRequestHeader::QueryParts::mutable_user_info, UF_USERINFO);
        mutable_request_header().set_method(http_method_str(static_cast<http_method>(parser->method)));
        mutable_request_header().set_version_major(parser->http_major);
        mutable_request_header().set_version_minor(parser->http_minor);
        _response_header->set_version_major(parser->http_major);
        _response_header->set_version_minor(parser->http_minor);
        BOOST_LOG_TRIVIAL(info) << "request_context::finalise_header - header complete:\n" << api::as_json(request_header());
    }
    
    void request_context::consume_body(const char *begin,
                                              std::size_t size)
    {
        asio::write(_request_stream, asio::buffer(begin, size));
        
    }
    
    void request_context::notify_eof()
    {
        _request_stream.close();
    }
    

    
}}}